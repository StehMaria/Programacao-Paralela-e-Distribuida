# Aula 6.5: Comunicação Não-Bloqueante e Sobreposição de Computação e Comunicação (`MPI_Isend`, `MPI_Irecv`, `MPI_Wait`, `MPI_Test`)

---

## 1. O Conceito de Comunicação Não-Bloqueante

Em aplicações de **Computação de Alto Desempenho (HPC)** em redes distribuídas, a transferência de dados entre nós através de placas de rede (InfiniBand, Ethernet) é ordens de grandeza mais lenta do que a velocidade de processamento da CPU.

Nas aulas anteriores, estudamos as chamadas de comunicação **bloqueantes** (`MPI_Send` e `MPI_Recv`). Quando um processo executa `MPI_Send` ou `MPI_Recv`, a CPU permanece parada (ou suspensa pelo sistema operacional) aguardando que o buffer seja copiado ou que a mensagem chegue. Em redes de alta latência, isso se traduz em **milhares de ciclos de CPU desperdiçados em tempo ocioso**.

A **Comunicação Não-Bloqueante** rompe esse gargalo ao separar o **início** de uma operação de comunicação da sua **conclusão**. 

Ao chamar uma função não-bloqueante (identificada pelo prefixo **`I`** de *Immediate*):
1. O processo solicita ao subsistema de rede/MPI que inicie a transferência.
2. A função **retorna imediatamente** (em nanossegundos), entregando um "comprovante" (*handle*) da solicitação.
3. A CPU fica livre para continuar executando cálculos pesados em paralelo enquanto o hardware de rede (DMA/NIC) realiza a transferência de dados em *background*.
4. Posteriormente, o processo verifica ou aguarda a conclusão da transferência usando o comprovante.

```
COMUNICAÇÃO BLOQUEANTE:
Processo: [--- CÁLCULO ---][======== ESPERA DE REDE ========][--- CÁLCULO ---]
Tempo Total: T_calculo + T_comunicacao

COMUNICAÇÃO NÃO-BLOQUEANTE (SOBREPOSIÇÃO / OVERLAPPING):
Processo: [--- CÁLCULO 1 ---][--- CÁLCULO INTERNO (SOBREPOSTO) ---][WAIT][--- CÁLCULO 2 ---]
Rede:                        [===== COMUNICAÇÃO DE BORDA =====]
Tempo Total: max(T_calculo_interno, T_comunicacao) + T_calculo_borda
```

---

### 💡 Analogia do Mundo Real: O Pedido da Pizza vs. Estudar para a Prova

- **Comunicação Bloqueante (`MPI_Send` / `MPI_Recv`):** Você está estudando e decide pedir uma pizza. Você faz o pedido por telefone, vai até a porta de casa, cruza os braços e **fica parado na porta olhando para a rua durante 40 minutos** até o entregador chegar. Você não lê nenhuma página do livro enquanto a pizza não estiver na sua mão.
- **Comunicação Não-Bloqueante (`MPI_Isend` / `MPI_Irecv`):** Você faz o pedido da pizza pelo aplicativo e recebe um **número de pedido / senha** (`MPI_Request`). Enquanto o entregador está a caminho pela cidade, você volta imediatamente para a sua escrivaninha e lê 3 capítulos do livro (*computação útil*). Quando você atinge um ponto da leitura onde precisa comer para continuar (*sincronização*), você vai até a porta conferir a senha e pegar a pizza (`MPI_Wait`).

---

## 2. Handshakes e Handles (`MPI_Request`)

Para gerenciar operações assíncronas que ocorrem em segundo plano, o MPI utiliza um tipo de dado especial chamado **`MPI_Request`**.

Quando você inicia uma chamada não-bloqueante, o MPI aloca uma estrutura interna para rastrear o estado do envio/recebimento e preenche a sua variável `MPI_Request`.

```c
MPI_Request request;
```

### ⚠️ A Regra de Ouro da Memória em Comunicação Assíncrona

> **NUNCA modifique ou leia o buffer de memória associado a uma operação não-bloqueante enquanto ela não for explicitamente concluída por um `MPI_Wait` ou `MPI_Test`.**

Se você chamar `MPI_Isend(buffer, ...)` e, na linha seguinte, alterar os valores de `buffer[0]` antes da conclusão do envio, a placa de rede transmitirá dados **corrompidos** ou parcialmente alterados. Da mesma forma, se você ler `buffer` após um `MPI_Irecv` sem antes esperar a conclusão, estará lendo lixo de memória.

---

## 3. As Funções de Inicialização Assíncrona

### 3.1 `MPI_Isend` (Immediate Send)

Inicia uma operação de envio não-bloqueante.

```c
int MPI_Isend(
    const void *buf,       // Endereço do buffer contendo os dados a enviar
    int count,             // Número de elementos
    MPI_Datatype datatype, // Tipo do dado
    int dest,              // Rank do processo destino
    int tag,               // Etiqueta/Tag da mensagem
    MPI_Comm comm,         // Comunicador (ex: MPI_COMM_WORLD)
    MPI_Request *request   // [SAÍDA] Ponteiro para o handle de requisição
);
```

### 3.2 `MPI_Irecv` (Immediate Receive)

Posta uma solicitação de recebimento não-bloqueante antes mesmo que a mensagem chegue (*posting a receive*).

```c
int MPI_Irecv(
    void *buf,             // Endereço do buffer que receberá os dados
    int count,             // Capacidade máxima de elementos
    MPI_Datatype datatype, // Tipo do dado
    int source,            // Rank da origem (ou MPI_ANY_SOURCE)
    int tag,               // Tag da mensagem (ou MPI_ANY_TAG)
    MPI_Comm comm,         // Comunicador
    MPI_Request *request   // [SAÍDA] Ponteiro para o handle de requisição
);
```

---

## 4. Sincronização e Verificação de Conclusão

Após iniciar operações assíncronas e realizar computação paralela independente, o programa precisa verificar se a comunicação terminou antes de usar os dados recebidos ou reutilizar os buffers de envio.

### 4.1 `MPI_Wait` (Bloqueio Bloqueante na Requisição)

Pausa a execução do processo até que a operação associada ao `request` seja **totalmente concluída**.

```c
int MPI_Wait(
    MPI_Request *request, // [ENTRADA/SAÍDA] Handle da requisição (definido como MPI_REQUEST_NULL ao concluir)
    MPI_Status *status    // [SAÍDA] Estrutura de status (ou MPI_STATUS_IGNORE)
);
```

### 4.2 `MPI_Test` (Sonda Não-Bloqueante)

Verifica se a operação associada ao `request` terminou, **sem bloquear** a execução.

```c
int MPI_Test(
    MPI_Request *request, // Handle da requisição
    int *flag,            // [SAÍDA] Retorna 1 (true) se concluído, 0 (false) se ainda pendente
    MPI_Status *status    // [SAÍDA] Status da operação
);
```

### 4.3 Gerenciamento de Múltiplas Requisições em Lote

Na prática, aplicações HPC trocam dados com múltiplos vizinhos simultaneamente. O MPI fornece funções altamente eficientes para gerenciar arrays de `MPI_Request`:

| Função | Comportamento | Uso Típico |
| :--- | :--- | :--- |
| **`MPI_Waitall(count, req_array, status_array)`** | Bloqueia até que **todas** as `count` requisições no array terminem. | Troca de bordas (*halo exchange*) completa antes da próxima fase. |
| **`MPI_Waitany(count, req_array, &index, status)`** | Bloqueia até que **pelo menos uma** requisição termine e retorna seu índice. | Processamento dinâmico à medida que dados chegam de qualquer nó. |
| **`MPI_Waitsome(incount, req_array, &outcount, indices, status_array)`** | Bloqueia até que **uma ou mais** requisições terminem, retornando a lista. | Processamento em lote de pacotes parciais prontos. |
| **`MPI_Testall / MPI_Testany / MPI_Testsome`** | Versões não-bloqueantes equivalentes das funções acima. | Laços de pesquisa (*polling loops*) com trabalho alternativo. |

---

## 5. A Técnica de Ocultamento de Latência na Prática (*Halo Exchange*)

A aplicação por excelência da comunicação não-bloqueante é a simulação em grade estruturada (ex: equação do calor, mecânica dos fluidos, processamento de imagem).

Nesses problemas, o domínio de dados é dividido entre os nós. Cada nó precisa atualizar suas células locais. No entanto, o cálculo das **células de borda** depende de valores das **células fantasma (*ghost cells / halo*)** dos nós vizinhos.

```
+------------------------------------+
|         GHOST CELLS (BORDA)        |  <-- Dados recebidos do Nó Vizinho
+------------------------------------+
|  CÉLULAS INTERNAS (INDEPENDENTES)  |  <-- Podem ser calculadas SEM esperar a rede!
|                                    |
|  Podem rodar em paralelo enquanto  |
|  a rede transfere a borda.         |
+------------------------------------+
|         GHOST CELLS (BORDA)        |  <-- Dados enviados ao Nó Vizinho
+------------------------------------+
```

### Estrutura do Algoritmo de Ocultamento de Latência:

1. **Disparo Assíncrono (`MPI_Isend` / `MPI_Irecv`):** Inicia a troca de bordas com os nós vizinhos.
2. **Computação do Miolo / Domínio Interno:** Calcula todas as células internas que **não dependem** das células fantasma.
3. **Sincronização (`MPI_Waitall`):** Aguarda o término da transferência de rede das bordas.
4. **Computação do Contorno / Borda:** Calcula o contorno do domínio utilizando as células fantasma recém-chegadas.

Essa técnica consegue ocultar 100% da latência de comunicação se o tempo de computação das células internas for maior que o tempo de transmissão de rede!

---

## 6. Exemplo Prático Completo em C

O código a seguir demonstra o padrão completo de comunicação não-bloqueante bidirecional entre dois nós (`Rank 0` e `Rank 1`), realizando computação útil sobreposta enquanto as mensagens cruzam a rede.

```c
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> // Para sleep() simulando cálculo

#define TAMANHO 1000000 // 1 milhão de floats (~4MB)

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (size < 2) {
        if (rank == 0) {
            printf("Erro: Este programa requer pelo menos 2 processos MPI.
");
        }
        MPI_Finalize();
        return 1;
    }

    // Alocação de buffers
    float *buffer_envio = (float*) malloc(TAMANHO * sizeof(float));
    float *buffer_recibo = (float*) malloc(TAMANHO * sizeof(float));

    // Inicializa dados do buffer de envio
    for (int i = 0; i < TAMANHO; i++) {
        buffer_envio[i] = (float)(rank + 1) * 10.0f;
    }

    int vizinho = (rank == 0) ? 1 : 0;
    
    MPI_Request requests[2];
    MPI_Status statuses[2];

    double t_inicio = MPI_Wtime();

    // 1. DISPARO ASSÍNCRONO DA COMUNICAÇÃO (NÃO-BLOQUEANTE)
    // Posta o recebimento primeiro
    MPI_Irecv(buffer_recibo, TAMANHO, MPI_FLOAT, vizinho, 100, MPI_COMM_WORLD, &requests[0]);
    // Posta o envio em seguida
    MPI_Isend(buffer_envio, TAMANHO, MPI_FLOAT, vizinho, 100, MPI_COMM_WORLD, &requests[1]);

    printf("[Rank %d] Comunicação assíncrona disparada! Executando trabalho pesado no miolo...
", rank);

    // 2. COMPUTAÇÃO ÚTIL SOBREPOSTA (Cálculo em dados internos independentes)
    double soma_interna = 0.0;
    for (int i = 0; i < 50000000; i++) {
        soma_interna += (i * 0.000001);
    }

    printf("[Rank %d] Trabalho interno concluído (Soma simulada = %.2f). Aguardando término da rede...
", rank, soma_interna);

    // 3. SINCRONIZAÇÃO (Espera a conclusão de ambas as operações)
    MPI_Waitall(2, requests, statuses);

    double t_fim = MPI_Wtime();

    printf("[Rank %d] Sincronização concluída! Primeiro elemento recebido do Rank %d: %.2f (Tempo total: %.4f s)
",
           rank, vizinho, buffer_recibo[0], t_fim - t_inicio);

    // Limpeza
    free(buffer_envio);
    free(buffer_recibo);

    MPI_Finalize();
    return 0;
}
```

### Roteiro de Compilação e Execução:

```bash
# Compilar
mpicc -O3 -o aula6_5_nonblocking aula6_5_nonblocking.c

# Executar com 2 processos distribuídos
mpirun -np 2 ./aula6_5_nonblocking
```

---

## 7. Síntese de Fechamento do Módulo 6 (Programação Distribuída com MPI)

Com esta aula, concluímos o estudo completo do paradigma de **Programação Distribuída por Passagem de Mensagens (MPI)**!

```
+---------------------------------------------------------------------------------+
|                                 SÍNTESE DO MÓDULO 6                             |
+---------------------------------------------------------------------------------+
| Aula 6.1: Modelo Distribuído & SPMD | Processos isolados em RAM, MPI_COMM_WORLD |
| Aula 6.2: Ponto a Ponto Bloqueante  | MPI_Send / MPI_Recv e prevenção de Deadlock|
| Aula 6.3: Comunicação Coletiva      | Bcast, Scatter, Gather, Reduce, Allreduce  |
| Aula 6.4: Tipos Derivados           | Structs, Vector, Commit e Padding de Memória|
| Aula 6.5: Não-Bloqueante            | Isend, Irecv, Waitall e Ocultamento Latência|
+---------------------------------------------------------------------------------+
```

---

## 8. Tabela Comparativa: Bloqueante vs. Não-Bloqueante

| Característica | Comunicação Bloqueante (`MPI_Send`/`Recv`) | Comunicação Não-Bloqueante (`MPI_Isend`/`Irecv`) |
| :--- | :--- | :--- |
| **Retorno da Função** | Retorna apenas quando o buffer está seguro/mensagem entregue. | Retorna **imediatamente**, antes da transferência terminar. |
| **Handle de Controle** | Não utiliza (`NULL`). | Exige obrigatoriamente um objeto **`MPI_Request`**. |
| **Segurança de Memória** | O buffer pode ser reutilizado imediatamente após a chamada. | Proibido tocar no buffer até rodar `MPI_Wait` ou `MPI_Test`. |
| **Uso de CPU** | CPU desperdiçada em espera ociosa de rede. | CPU livre para realizar cálculos em paralelo. |
| **Aplicação Ideal** | Troca simples de mensagens pequenas e controle. | Simulações em grade, grandes matrizes e ocultamento de latência. |

---

## 9. Exercícios de Fixação

### Questão 1
Qual é a principal vantagem do uso de `MPI_Isend` e `MPI_Irecv` em relação às suas contrapartidas bloqueantes?
- A) Reduzir o número de bytes transmitidos pela placa de rede.
- B) Permitir a sobreposição entre tempo de computação da CPU e tempo de comunicação de rede.
- C) Eliminar a necessidade de declarar variáveis do tipo `MPI_Datatype`.
- D) Garantir que a ordem de chegada das mensagens seja estritamente sequencial.

### Questão 2
O que acontece se um programa modificar o conteúdo de um vetor de floats logo na linha seguinte à chamada `MPI_Isend(vetor, ...)` sem executar `MPI_Wait`?
- A) O compilador emite um erro de sintaxe e interrompe a compilação.
- B) O MPI cancela o envio automaticamente para proteger a memória.
- C) Ocorre condição de corrida de memória (*data race*), resultando no envio de dados corrompidos para o destino.
- D) A chamada `MPI_Isend` se transforma automaticamente em `MPI_Send` bloqueante.

### Questão 3
Qual função deve ser utilizada quando um processo possui um array de 4 estruturas `MPI_Request` resultantes de trocas com vizinhos do norte, sul, leste e oeste, e precisa garantir que TODAS foram concluídas antes de avançar para a próxima iteração?
- A) `MPI_Waitany`
- B) `MPI_Test`
- C) `MPI_Waitall`
- D) `MPI_Barrier`

---

### Gabarito Comentado

- **Questão 1: Resposta B.** A grande vantagem da comunicação assíncrona é liberar a CPU para realizar cálculos úteis enquanto a placa de rede efetua a transferência dos dados em segundo plano, ocultando a latência do barramento.
- **Questão 2: Resposta C.** Como a função `MPI_Isend` retorna imediatamente enquanto a placa de rede lê a memória em background, alterar o buffer antes da conclusão resulta na leitura de dados inconsistentes ou corrompidos durante a transmissão.
- **Questão 3: Resposta C.** A função `MPI_Waitall` recebe um array de requisições e a quantidade (4) e bloqueia o processo até que todas as 4 transferências associadas tenham sido concluídas com sucesso.
