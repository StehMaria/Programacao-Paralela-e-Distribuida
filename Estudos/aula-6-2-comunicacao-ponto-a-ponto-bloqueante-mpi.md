# Aula 6.2: Comunicação Ponto a Ponto Bloqueante no MPI — `MPI_Send`, `MPI_Recv`, Semântica de Buffers, Tags e Deadlocks

## Visão Geral e Objetivos de Aprendizagem

Nesta aula, avançaremos do setup básico do ambiente MPI para a **comunicação ponto a ponto (*Point-to-Point Communication*)**, a forma mais fundamental de troca de informações entre dois processos específicos em um sistema de memória distribuída.

Ao final desta aula, você será capaz de:
1. Compreender a semântica e a assinatura das funções bloqueantes `MPI_Send` e `MPI_Recv`.
2. Dominar a estrutura e a utilidade dos **Tipos de Dados do MPI (`MPI_Datatype`)** e das **Etiquetas de Mensagem (`Tags`)**.
3. Inspecionar mensagens recebidas e extrair metadados usando a estrutura `MPI_Status`.
4. Identificar e prever **Deadlocks em Comunicação Distribuída**, entendendo o comportamento dos buffers do sistema.
5. Aplicar estratégias de prevenção de Deadlocks, incluindo a reordenação de chamadas e a utilização da rotina atômica combinada `MPI_Sendrecv`.

---

## 1. O Conceito de Comunicação Ponto a Ponto

Enquanto no modelo de memória compartilhada (como OpenMP) as *threads* trocam dados escrevendo e lendo em endereços de RAM comum, no **MPI** cada processo vive em seu próprio espaço de endereçamento isolado. Portanto, para transmitir um dado do **Processo A** para o **Processo B**, o Processo A precisa explicitamente **enviar** uma cópia e o Processo B precisa explicitamente **receber** essa cópia.

```
+--------------------------------+           +--------------------------------+
|          PROCESSO 0            |           |          PROCESSO 1            |
|  [ Memória Privada P0 ]        |           |  [ Memória Privada P1 ]        |
|                                |           |                                |
|   int valor = 42;              |           |   int valor_recebido;          |
|   MPI_Send(&valor, ...) -------|---Rede--->|---MPI_Recv(&valor_recebido,...) |
+--------------------------------+           +--------------------------------+
```

### Analogia do Correio Registrado
Pense na comunicação ponto a ponto como o envio de um **pacote com aviso de recebimento**:
* **O Remetente (`MPI_Send`)** embala o objeto em uma caixa, coloca uma etiqueta indicando o conteúdo e o endereço de destino, e entrega o pacote no balcão dos correios.
* **O Destinatário (`MPI_Recv`)** precisa ir até a portaria do seu prédio, apresentar sua identificação e esperar o entregador trazer a caixa para poder abri-la e usar o conteúdo.

---

## 2. Anatomia Detalhada de `MPI_Send` e `MPI_Recv`

Para que uma mensagem seja transmitida sem ambiguidades pela rede, o MPI precisa saber **6 informações fundamentais**:
1. **Onde** estão os dados na memória? (*Ponteiro para o buffer*)
2. **Quantos** elementos serão enviados/recebidos? (*Contagem*)
3. **Qual é o tipo** de cada elemento? (*MPI_Datatype*)
4. **Quem** é o destinatário/remetente? (*Rank do processo*)
5. **Qual é o assunto** ou categoria da mensagem? (*Tag*)
6. **Qual é o contexto** de comunicação? (*Comunicador*)

### 2.1. Assinatura do `MPI_Send`

```c
int MPI_Send(
    const void *buf,       // 1. Ponteiro para o início dos dados a enviar
    int count,             // 2. Número de elementos no buffer
    MPI_Datatype datatype, // 3. Tipo do dado (ex: MPI_INT, MPI_DOUBLE)
    int dest,              // 4. Rank do processo de destino (0 a N-1)
    int tag,               // 5. Etiqueta da mensagem (inteiro >= 0)
    MPI_Comm comm          // 6. Comunicador (ex: MPI_COMM_WORLD)
);
```

### 2.2. Assinatura do `MPI_Recv`

```c
int MPI_Recv(
    void *buf,             // 1. Ponteiro para a memória onde os dados serão salvos
    int count,             // 2. Capacidade MÁXIMA do buffer de recepção
    MPI_Datatype datatype, // 3. Tipo do dado esperado
    int source,            // 4. Rank do remetente (ou MPI_ANY_SOURCE)
    int tag,               // 5. Etiqueta esperada (ou MPI_ANY_TAG)
    MPI_Comm comm,         // 6. Comunicador (ex: MPI_COMM_WORLD)
    MPI_Status *status     // 7. Estrutura com detalhes sobre a mensagem recebida
);
```

---

## 3. Tipos de Dados e Etiquetas (Tags)

### 3.1. Mapeamento de Tipos de Dados (`MPI_Datatype`)

Como cada nó do cluster pode ter arquiteturas heterogêneas (ex: diferenças de *endianness* ou alinhamento), o MPI define seus próprios tipos de dados para garantir a conversão transparente durante a transmissão pela rede:

| Tipo em C | Tipo Equivalente no MPI | Tamanho Típico |
| :--- | :--- | :--- |
| `char` | `MPI_CHAR` / `MPI_SIGNED_CHAR` | 1 byte |
| `int` | `MPI_INT` | 4 bytes |
| `float` | `MPI_FLOAT` | 4 bytes |
| `double` | `MPI_DOUBLE` | 8 bytes |
| `long` | `MPI_LONG` | 4 ou 8 bytes |
| Byte Bruto / Genérico | `MPI_BYTE` | 1 byte irrelevante a tipo |

### 3.2. O Papel da Etiqueta (`Tag`)

A **Tag** é um número inteiro (geralmente entre $0$ e $32767$) escolhido pelo programador para servir como o **"assunto do e-mail"**. 

Se o Processo 0 enviar duas mensagens consecutivas para o Processo 1 — uma contendo a **temperatura** (`tag = 100`) e outra contendo a **pressão** (`tag = 200`) —, o Processo 1 pode usar a Tag para garantir que lerá a mensagem de pressão primeiro, mesmo que a de temperatura tenha chegado antes na placa de rede!

---

## 4. O Coringa Corrente e a Estrutura `MPI_Status`

### 4.1. Coringas de Recepção (`MPI_ANY_SOURCE` e `MPI_ANY_TAG`)

Às vezes, um processo mestre precisa receber resultados de qualquer trabalhador que terminar o processamento primeiro. Para isso, o MPI oferece os valores coringa:
* `source = MPI_ANY_SOURCE`: Aceita mensagens vindas de **qualquer** processo.
* `tag = MPI_ANY_TAG`: Aceita mensagens com **qualquer** etiqueta.

### 4.2. Inspecionando a Mensagem com `MPI_Status`

Quando usamos coringas, como descobrimos **quem** enviou a mensagem ou **quantos** dados realmente chegaram? Usamos o argumento `MPI_Status`:

```c
MPI_Status status;
MPI_Recv(buffer, 100, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

// Extraindo informações do status:
int remetente = status.MPI_SOURCE; // Descobre quem enviou
int etiqueta  = status.MPI_TAG;    // Descobre a tag utilizada

// Descobrindo a quantidade exata de elementos recebidos:
int qtd_recebida;
MPI_Get_count(&status, MPI_INT, &qtd_recebida);

printf("Recebi %d inteiros do Processo %d com a Tag %d\n", qtd_recebida, remetente, etiqueta);
```

---

## 5. Semântica de Bloqueio e Buffers Internos

Entender o que significa ser **"Bloqueante"** é o passo mais crucial para evitar travamentos em sistemas distribuídos.

```
[ Indicação de Imagem: Diagrama mostrando a linha do tempo de execução de MPI_Send e MPI_Recv.
  O Processo 0 chama MPI_Send. O dado é copiado para o Buffer de Saída do Sistema.
  O Processo 0 é liberado. O Processo 1 chama MPI_Recv e fica BLOQUEADO aguardando
  a chegada dos pacotes de rede antes de copiar para o seu buffer de aplicação. ]
```

### O que significa ser Bloqueante?
* **`MPI_Recv` é estritamente bloqueante:** Ele **NÃO** retorna até que a mensagem chegue e seja completamente copiada para o buffer de memória da aplicação (`buf`). Se a mensagem nunca for enviada, o processo fica suspenso indefinidamente.
* **`MPI_Send` é bloqueante de recursos:** Ele só retorna quando o buffer de aplicação do remetente (`buf`) puder ser reutilizado com segurança sem alterar os dados que estão sendo transmitidos.

### Modo Bufferizado vs. Modo Síncrono (*Rendez-vous*)
O comportamento exato do `MPI_Send` depende do tamanho da mensagem e dos buffers do sistema operacional:
1. **Mensagens Pequenas (Modo Eager / Bufferizado):** O MPI copia a mensagem para um buffer interno de rede e o `MPI_Send` retorna imediatamente, mesmo antes do destinatário ter chamado `MPI_Recv`.
2. **Mensagens Grandes (Modo Síncrono / Rendez-vous):** O MPI não aloca buffers gigantes do sistema. O `MPI_Send` é obrigado a esperar até que o destinatário inicie o `MPI_Recv` e confirme o aperto de mão (*handshake*).

---

## 6. O Fantasma do Deadlock em MPI e Como Evitá-lo

O **Deadlock (Impasse Fatal)** em MPI ocorre quando dois ou mais processos ficam bloqueados esperando uns pelos outros em um ciclo sem fim.

### 6.1. O Cenário Clássico de Deadlock

Imagine que o Processo 0 e o Processo 1 querem trocar um vetor de dados gigante entre si:

```c
// CÓDIGO PERIGOSO — GERA DEADLOCK COM MENSAGENS GRANDES!

// Processo 0:
MPI_Send(vetor_envio, GIGANTE, MPI_INT, 1, 0, MPI_COMM_WORLD);
MPI_Recv(vetor_rec,   GIGANTE, MPI_INT, 1, 0, MPI_COMM_WORLD, &status);

// Processo 1:
MPI_Send(vetor_envio, GIGANTE, MPI_INT, 0, 0, MPI_COMM_WORLD);
MPI_Recv(vetor_rec,   GIGANTE, MPI_INT, 0, 0, MPI_COMM_WORLD, &status);
```

**Por que ocorre o Deadlock?**
Como a mensagem é grande, nem o Processo 0 nem o Processo 1 conseguem bufferizar os dados. Ambos chamam `MPI_Send` e ficam **bloqueados** aguardando o outro lado chamar `MPI_Recv`. Como nenhum dos dois chega na linha do `MPI_Recv`, o programa trava para sempre (0% de uso de CPU, aguardando rede).

---

### 6.2. Estratégias de Solução

#### Solução 1: Reordenação Mútua de Chamadas (Inversão Par/Ímpar)
Invertemos a ordem das chamadas em um dos processos de forma que um `MPI_Send` sempre encontre um `MPI_Recv` correspondente:

```c
if (rank == 0) {
    MPI_Send(vetor_envio, GIGANTE, MPI_INT, 1, 0, MPI_COMM_WORLD);
    MPI_Recv(vetor_rec,   GIGANTE, MPI_INT, 1, 0, MPI_COMM_WORLD, &status);
} else if (rank == 1) {
    MPI_Recv(vetor_rec,   GIGANTE, MPI_INT, 0, 0, MPI_COMM_WORLD, &status);
    MPI_Send(vetor_envio, GIGANTE, MPI_INT, 0, 0, MPI_COMM_WORLD);
}
```

#### Solução 2: A Solução Elegante com `MPI_Sendrecv`
O MPI oferece a rotina combinada `MPI_Sendrecv`, que realiza o envio e a recepção de dados simultaneamente de forma **atômica e segura contra deadlocks**, gerenciando os buffers internamente:

```c
int MPI_Sendrecv(
    const void *sendbuf, int sendcount, MPI_Datatype sendtype, int dest, int sendtag,
    void *recvbuf, int recvcount, MPI_Datatype recvtype, int source, int recvtag,
    MPI_Comm comm, MPI_Status *status
);
```

---

## 7. Exemplo Prático Completo em C: O Jogo de Ping-Pong

O código a seguir implementa um teste de **Ping-Pong** entre o Processo 0 e o Processo 1. O número de rebate é incrementado a cada troca até atingir o limite definido.

```c
#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>

#define LIMITE_PING_PONG 10

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    // Este programa precisa de pelo menos 2 processos
    if (size < 2) {
        if (rank == 0) {
            fprintf(stderr, "Erro: Este programa precisa de pelo menos 2 processos MPI.\n");
        }
        MPI_Finalize();
        return 1;
    }

    int contador = 0;
    int parceiro = (rank == 0) ? 1 : 0; // Se sou 0, parceiro é 1; se sou 1, parceiro é 0.

    while (contador < LIMITE_PING_PONG) {
        if (rank == contador % 2) {
            // É a minha vez de sacar (enviar)
            contador++;
            MPI_Send(&contador, 1, MPI_INT, parceiro, 0, MPI_COMM_WORLD);
            printf("Processo %d enviou jogada %d para o Processo %d\n", rank, contador, parceiro);
        } else {
            // É a minha vez de rebater (receber)
            MPI_Status status;
            MPI_Recv(&contador, 1, MPI_INT, parceiro, 0, MPI_COMM_WORLD, &status);
            printf("Processo %d recebeu jogada %d do Processo %d\n", rank, contador, parceiro);
        }
    }

    MPI_Finalize();
    return 0;
}
```

### Roteiro de Compilação e Execução

```bash
# 1. Compilar o código
mpicc -O2 ping_pong.c -o ping_pong

# 2. Executar com 2 processos no cluster local
mpirun -np 2 ./ping_pong
```

---

## 8. Tabela Comparativa de Modos de Comunicação Ponto a Ponto

| Função | Comportamento do Envio | Risco de Deadlock | Aplicação Típica |
| :--- | :--- | :--- | :--- |
| `MPI_Send` | Bloqueia até que o buffer do remetente possa ser reusado. | **ALTO** se mal ordenado com mensagens grandes. | Troca genérica simples. |
| `MPI_Ssend` *(Synchronous)* | Bloqueia até que o receptor INICIE o `MPI_Recv` (handshake). | **MUITO ALTO** (útil para debug de sincronização). | Testes de acoplamento rígido. |
| `MPI_Bsend` *(Buffered)* | Copia os dados para um buffer alocado manualmente pelo usuário e retorna imediatamente. | **BAIXO** (exige `MPI_Buffer_attach`). | Quando a velocidade do remetente não pode esperar o receptor. |
| `MPI_Sendrecv` | Envia e recebe dados em um único passo atômico e otimizado. | **ZERO** (Gerenciado nativamente pelo runtime MPI). | Troca de bordas (*Halo Exchange*) em malhas 2D/3D. |

---

## Exercícios de Fixação com Gabarito Comentado

### Questão 1
Por que a chamada `MPI_Recv(buffer, 10, MPI_INT, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &status)` pode ser considerada não-determinística quanto ao remetente, e como o programador pode descobrir exatamente qual processo enviou a mensagem?

* **Gabarito Comentado:**
  A chamada é não-determinística porque a constante `MPI_ANY_SOURCE` aceita mensagens vindas de qualquer processo que envie com a tag `0`. O processo que transmitir primeiro pela rede terá sua mensagem capturada. Para identificar o remetente exato após a recepção, o programador deve inspecionar o campo `status.MPI_SOURCE` da estrutura `MPI_Status` passada por referência para o `MPI_Recv`.

---

### Questão 2
Dois processos executam o seguinte trecho de código:
* Processo 0: `MPI_Send(bufA, 1000000, MPI_DOUBLE, 1, 0, MPI_COMM_WORLD);` seguido de `MPI_Recv(bufB, 1000000, MPI_DOUBLE, 1, 0, MPI_COMM_WORLD, &st);`
* Processo 1: `MPI_Send(bufB, 1000000, MPI_DOUBLE, 0, 0, MPI_COMM_WORLD);` seguido de `MPI_Recv(bufA, 1000000, MPI_DOUBLE, 0, 0, MPI_COMM_WORLD, &st);`

Explique por que este código pode funcionar perfeitamente para vetores pequenos (ex: 5 elementos), mas trava em Deadlock para vetores grandes (ex: 1.000.000 de elementos).

* **Gabarito Comentado:**
  Para vetores pequenos, o runtime do MPI utiliza o modo de envio bufferizado (*Eager Mode*), copiando a mensagem para um buffer temporário interno do sistema e liberando o `MPI_Send` imediatamente em ambos os processos. Assim, ambos progridem para a linha do `MPI_Recv`. Contudo, para vetores grandes (1.000.000 de `double`s = 8 MB), o MPI desativa o buffer interno para economizar RAM e passa a usar o protocolo *Rendez-vous* (síncrono). Ambos os processos tentam enviar primeiro e ficam bloqueados esperando o outro chamar `MPI_Recv`. Como nenhum alcança a linha de recepção, ocorre um Deadlock clássico.

---

### Questão 3
Qual é a principal vantagem de substituir um par de chamadas `MPI_Send` / `MPI_Recv` pela função unificada `MPI_Sendrecv` na implementação da troca de condições de contorno (*Halo Exchange*) em uma simulação numérica?

* **Gabarito Comentado:**
  A principal vantagem é a eliminação completa do risco de Deadlock de comunicação de forma limpa e otimizada. O `MPI_Sendrecv` coordena internamente o envio e a recepção de dados entre nós adjacentes, permitindo que a placa de rede opere em modo *Full-Duplex* (enviando e recebendo pacotes simultaneamente), sem que o programador precise escrever lógicas complexas de ordenação Par/Ímpar.

---

Com o domínio da **Comunicação Ponto a Ponto Bloqueante**, você possui as bases essenciais para trocar mensagens entre nós em sistemas distribuídos. Na próxima aula (**Aula 6.3**), exploraremos a **Comunicação Coletiva no MPI** (`MPI_Bcast`, `MPI_Scatter`, `MPI_Gather` e `MPI_Reduce`)!
