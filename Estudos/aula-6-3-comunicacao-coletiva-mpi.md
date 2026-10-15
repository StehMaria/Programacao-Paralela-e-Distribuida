# Aula 6.3: Comunicação Coletiva no MPI — `MPI_Bcast`, `MPI_Scatter`, `MPI_Gather`, `MPI_Reduce` e `MPI_Allreduce`

---

## 1. Introdução: O Poder da Ação Coletiva

Nas aulas anteriores, aprendemos a fazer a comunicação **ponto a ponto** (*Point-to-Point*) usando `MPI_Send` e `MPI_Recv`. É o equivalente a trocar e-mails ou fazer chamadas telefônicas individuais entre dois nós específicos do cluster.

Contudo, na maioria das aplicações paralelas de alto desempenho (como simulações de mecânica dos fluidos, inteligência artificial distribuída ou multiplicação de matrizes gigantes), precisamos que **todos os processos do cluster colaborem simultaneamente** em uma operação de dados.

É aqui que entram as **Operações de Comunicação Coletiva** do MPI:

> 💡 **Definição de Comunicação Coletiva:**
> Uma rotina coletiva é uma função que **DEVE ser chamada por TODOS os processos** pertencentes a um determinado comunicador (como `MPI_COMM_WORLD`). Se um único processo falhar em chamar a rotina coletiva, o programa ficará travado indefinidamente (*Deadlock*).

### A Analogia da Reunião de Empresa vs. E-mails Individuais
Imagine que um diretor precisa enviar uma diretriz para 100 funcionários:
* **Abordagem Ponto a Ponto (`MPI_Send`/`MPI_Recv`):** O diretor digita 100 e-mails individuais, um por um. Além de demorado, ele gasta $O(N)$ do seu próprio tempo.
* **Abordagem Coletiva (`MPI_Bcast`):** O diretor usa um sistema de alto-falante ou convocação geral. Todos ouvem ao mesmo tempo. Internamente, a infraestrutura usa uma **árvore de distribuição binária**, transmitindo a informação em tempo $O(\log_2 N)$.

```
   [Comunicação Ponto a Ponto (Laço Manual)]          [Comunicação Coletiva (Árvore O(log N))]
            Processo 0 (Root)                                    Processo 0 (Root)
         /      |      |      \                                    /                Proc 1  Proc 2  Proc 3  Proc 4                             Proc 1       Proc 2
                                                                /      \     /                                                                   Proc 3  Proc 4 Proc 5  Proc 6
```

---

## 2. Categorias de Operações Coletivas

As operações coletivas no MPI são divididas em 3 grandes famílias, dependendo do fluxo de movimentação dos dados:

```
+-----------------------------------------------------------------------------------+
|                            OPERAÇÕES COLETIVAS NO MPI                             |
+------------------------------+----------------------------------+-----------------+
| 1-para-Muitos (Um para Todos)| Muitos-para-1 (Todos para Um)   | Muitos-para-Muitos|
+------------------------------+----------------------------------+-----------------+
| • MPI_Bcast (Transmitir)     | • MPI_Gather (Coletar/Juntar)    | • MPI_Allreduce |
| • MPI_Scatter (Espalhar)     | • MPI_Reduce (Reduzir/Combinar)  | • MPI_Allgather |
|                              |                                  | • MPI_Alltoall  |
+------------------------------+----------------------------------+-----------------+
```

---

## 3. Operações 1-para-Muitos (Um para Todos)

Nessas operações, **um único processo raiz (*Root*)** possui os dados e deseja transmiti-los para todos os outros nós do cluster.

---

### A. `MPI_Bcast` (Broadcast / Transmissão Geral)

O `MPI_Bcast` replica exatamente o **mesmo bloco de dados** que está no processo *root* para a memória de **todos** os outros processos no comunicador.

#### Visualização do Diagrama
*(Referência visual do arquivo de slides `MPI.pdf` e `14_mpi.pdf`)*

```
Antes do Bcast (Root = 0):
Processo 0: [ A | B | C | D ]  (Dados originais)
Processo 1: [   |   |   |   ]  (Buffer vazio/lixo)
Processo 2: [   |   |   |   ]  (Buffer vazio/lixo)
Processo 3: [   |   |   |   ]  (Buffer vazio/lixo)

Após MPI_Bcast:
Processo 0: [ A | B | C | D ]
Processo 1: [ A | B | C | D ]
Processo 2: [ A | B | C | D ]
Processo 3: [ A | B | C | D ]
```

#### Assinatura da Função em C
```c
int MPI_Bcast(
    void* buffer,           // Endereço da memória (envio no Root, recepção nos demais)
    int count,              // Número de elementos
    MPI_Datatype datatype,  // Tipo de dado (ex: MPI_INT, MPI_DOUBLE)
    int root,               // Rank do processo de origem
    MPI_Comm comm           // Comunicador (ex: MPI_COMM_WORLD)
);
```

> ⚠️ **Atenção:** Repare que o parâmetro é único (`buffer`). No processo *root*, esse buffer contém os dados a serem lidos. Nos outros processos, o mesmo parâmetro aponta para o local onde os dados recebidos serão gravados!

#### Analogia do Mundo Real
O professor entra na sala e entrega uma cópia do **mesmo enunciado da prova** para cada um dos alunos.

---

### B. `MPI_Scatter` (Espalhar / Fatiar e Distribuir)

Diferente do Broadcast (que envia cópias idênticas), o `MPI_Scatter` pega um vetor/array do processo *root*, **divide-o em fatias iguais** e envia uma fatia diferente para cada processo.

#### Visualização do Diagrama
```
Antes do Scatter (Root = 0):
Processo 0: [ A | B | C | D ]  (Vetor completo de tamanho 4)
Processo 1: [   ]
Processo 2: [   ]
Processo 3: [   ]

Após MPI_Scatter (count = 1 para cada):
Processo 0 recebe: [ A ]
Processo 1 recebe: [ B ]
Processo 2 recebe: [ C ]
Processo 3 recebe: [ D ]
```

#### Assinatura da Função em C
```c
int MPI_Scatter(
    const void* sendbuf,    // Buffer de envio (relevante APENAS no Root)
    int sendcount,          // Número de elementos ENVIADOS PARA CADA PROCESSO
    MPI_Datatype sendtype,  // Tipo do dado de envio
    void* recvbuf,          // Buffer de recepção (usado por TODOS os processos)
    int recvcount,          // Número de elementos RECEBIDOS POR CADA PROCESSO
    MPI_Datatype recvtype,  // Tipo do dado de recepção
    int root,               // Rank do processo distribuidor
    MPI_Comm comm           // Comunicador
);
```

> 🎯 **Ponto Crítico:** O parâmetro `sendcount` indica quantos elementos são enviados **PARA CADA PROCESSO**, e NÃO o tamanho total do array! Se você tem 4 processos e um array de 100 elementos no root, o `sendcount` deve ser `25`.

#### Analogia do Mundo Real
Um carteiro chega com um maço de 4 cartas numeradas de 1 a 4 e entrega a carta 1 para a casa A, a carta 2 para a casa B, a carta 3 para a casa C e a carta 4 para a casa D.

---

## 4. Operações Muitos-para-1 (Todos para Um)

Nessas operações, **todos os processos** possuem dados locais e os enviam para consolidação em um **único processo raiz (*Root*)**.

---

### A. `MPI_Gather` (Coletar / Juntar Fatias)

O `MPI_Gather` é o **inverso exato do `MPI_Scatter`**. Ele pega uma fatia de dado de cada processo e as concatena em ordem crescente de *rank* dentro de um único vetor gigante no processo *root*.

#### Visualização do Diagrama
```
Antes do Gather:
Processo 0 possui: [ A ]
Processo 1 possui: [ B ]
Processo 2 possui: [ C ]
Processo 3 possui: [ D ]

Após MPI_Gather (Root = 0):
Processo 0 recebe: [ A | B | C | D ]  (Buffer de recepção montado em ordem de Rank)
Processo 1 possui: [ B ]
Processo 2 possui: [ C ]
Processo 3 possui: [ D ]
```

#### Assinatura da Função em C
```c
int MPI_Gather(
    const void* sendbuf,    // Buffer com o dado local de cada processo
    int sendcount,          // Quantidade enviada por cada processo
    MPI_Datatype sendtype,  
    void* recvbuf,          // Buffer de recepção final (alocado APENAS no Root)
    int recvcount,          // Quantidade RECEBIDA DE CADA PROCESSO
    MPI_Datatype recvtype,  
    int root,               // Rank do processo receptor
    MPI_Comm comm           
);
```

---

### B. `MPI_Reduce` (Redução / Combinação de Dados)

O `MPI_Reduce` pega dados de todos os processos e **os combina aritmeticamente ou logicamente** (por exemplo, somando todos, multiplicando ou encontrando o valor máximo), entregando o resultado final consolidado apenas no processo *root*.

#### Visualização do Diagrama
```
Antes da Redução (Operação MPI_SUM):
Processo 0 possui localmente:  10
Processo 1 possui localmente:  20
Processo 2 possui localmente:  15
Processo 3 possui localmente:  5

Após MPI_Reduce (Root = 0, Op = MPI_SUM):
Processo 0 recebe:  50   (10 + 20 + 15 + 5)
Processo 1 possui:  20
Processo 2 possui:  15
Processo 3 possui:  5
```

#### Assinatura da Função em C
```c
int MPI_Reduce(
    const void* sendbuf,    // Dado parcial local
    void* recvbuf,          // Buffer do resultado final (relevante APENAS no Root)
    int count,              // Número de elementos
    MPI_Datatype datatype,  
    MPI_Op op,              // Operação de redução (ex: MPI_SUM, MPI_MAX)
    int root,               // Rank do processo que receberá o total
    MPI_Comm comm           
);
```

#### Operadores Pré-definidos do MPI (`MPI_Op`)
| Operador MPI | Descrição da Operação |
| :--- | :--- |
| `MPI_SUM` | Soma dos valores |
| `MPI_PROD` | Produto (multiplicação) |
| `MPI_MAX` | Maior valor (*Maximum*) |
| `MPI_MIN` | Menor valor (*Minimum*) |
| `MPI_LAND` | E lógico (*Logical AND*) |
| `MPI_BAND` | E bit a bit (*Bitwise AND*) |
| `MPI_MAXLOC` | Encontra o valor máximo e o rank do nó correspondente |

---

## 5. Operações Muitos-para-Muitos (Todos para Todos)

Nestas operações, os dados são processados e **o resultado final é disponibilizado para TODOS os processos**, eliminando a necessidade de um segundo passo de transmissão.

---

### A. `MPI_Allreduce` (Reduzir e Transmitir para Todos)

O `MPI_Allreduce` executa uma redução idêntica ao `MPI_Reduce`, porém **distribui o resultado final para TODOS os processos no comunicador**. É funcionalmente equivalente a executar um `MPI_Reduce` seguido de um `MPI_Bcast`.

#### Visualização do Diagrama
```
Antes do Allreduce (Operação MPI_MAX):
Processo 0 possui:  4
Processo 1 possui:  9
Processo 2 possui:  2
Processo 3 possui:  7

Após MPI_Allreduce (Op = MPI_MAX):
Processo 0 recebe:  9
Processo 1 recebe:  9
Processo 2 recebe:  9
Processo 3 recebe:  9
```

> ⚡ **Dica de Ouro:** Observe que o `MPI_Allreduce` **NÃO possui o parâmetro `root`**, pois não há um processo especial de destino; todos os nós recebem o resultado!

#### Assinatura da Função em C
```c
int MPI_Allreduce(
    const void* sendbuf,    // Dado parcial local
    void* recvbuf,          // Buffer de resultado (recebido por TODOS)
    int count,              
    MPI_Datatype datatype,  
    MPI_Op op,              // Operação
    MPI_Comm comm           
);
```

---

### B. `MPI_Allgather` e `MPI_Alltoall`

* **`MPI_Allgather`:** Coleta os dados de todos e disponibiliza o array completo concatenado na memória de todos os nós (semelhante a `MPI_Gather` + `MPI_Bcast`).
* **`MPI_Alltoall`:** Cada processo envia dados distintos para cada um dos outros processos. É a transposição de matrizes totalmente distribuída (*Personalized All-to-All*).

---

## 6. Sincronização Coletiva Explicita: `MPI_Barrier`

Assim como o `#pragma omp barrier` no OpenMP, o MPI possui uma barreira de sincronização explícita para o modelo de memória distribuída:

```c
int MPI_Barrier(MPI_Comm comm);
```

Quando um processo chama `MPI_Barrier`, ele para a sua execução e **aguarda até que TODOS os outros processos do comunicador também alcancem essa mesma linha**.

> ⚠️ **Aviso de Desempenho:** Use o `MPI_Barrier` com extrema cautela. Ele força os nós mais rápidos a ficarem ociosos esperando pelos mais lentos, aumentando a degradação de desempenho por assimetria de carga. As rotinas de comunicação coletiva (`Bcast`, `Reduce`, etc.) já possuem mecanismos internos de coordenação e **não exigem** uma chamada a `MPI_Barrier` antes ou depois delas!

---

## 7. Exemplo Prático Completo em C: Cálculo do $\pi$ por Integração Numérica com MPI

Vamos consolidar esse aprendizado com o clássico problema da aproximação da constante $\pi$ através da regra do ponto médio para calcular a integral:

$$\int_{0}^{1} rac{4}{1 + x^2} dx = \pi$$

Neste código, usaremos:
1. `MPI_Bcast` para enviar o número de intervalos $N$ para todos os processos.
2. Divisão de trabalho baseada no `rank`.
3. `MPI_Reduce` para somar as áreas parciais calculadas por cada nó.

```c
#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>

int main(int argc, char** argv) {
    int rank, size;
    long num_steps = 100000000; // 100 milhões de retângulos
    double step, x, sum = 0.0, pi = 0.0;
    double start_time, end_time;

    // 1. Inicializa o ambiente MPI
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    start_time = MPI_Wtime();

    // 2. O Processo Root (0) pode ler ou definir o número de passos
    if (rank == 0) {
        printf("--- CÁLCULO DISTRIBUÍDO DO PI COM MPI ---
");
        printf("Número total de processos: %d
", size);
        printf("Número de retângulos (N): %ld
", num_steps);
    }

    // 3. BROADCAST: Transmite num_steps do Root para TODOS os processos
    MPI_Bcast(&num_steps, 1, MPI_LONG, 0, MPI_COMM_WORLD);

    // Largura de cada retângulo
    step = 1.0 / (double) num_steps;

    // 4. CÁLCULO PARCIAL (Divisão Cíclica de Iterações)
    // Cada processo calcula as fatias: rank, rank + size, rank + 2*size, ...
    for (long i = rank; i < num_steps; i += size) {
        x = (i + 0.5) * step;
        sum += 4.0 / (1.0 + x * x);
    }

    double local_pi = sum * step;

    // 5. REDUCE: Soma os resultados parciais (local_pi) no processo Root
    MPI_Reduce(&local_pi, &pi, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);

    end_time = MPI_Wtime();

    // 6. O Processo Root exibe o resultado final
    if (rank == 0) {
        printf("Valor calculado de PI: %.15f
", pi);
        printf("Tempo total de execução: %.6f segundos
", end_time - start_time);
    }

    // 7. Finaliza o ambiente MPI
    MPI_Finalize();
    return 0;
}
```

### Roteiro de Compilação e Execução
```bash
# Compilar o código
mpicc -O3 -o calcula_pi_mpi calcula_pi_mpi.c

# Executar com 4 processos no cluster/máquina local
mpirun -np 4 ./calcula_pi_mpi
```

---

## 8. Tabela Comparativa Consolidada

| Função MPI | Direção dos Dados | Envio (`sendbuf`) | Recepção (`recvbuf`) | Exige `root`? |
| :--- | :--- | :--- | :--- | :---: |
| **`MPI_Bcast`** | $1 ightarrow M$ | Dados no Root | Sobrescrito em todos | **Sim** |
| **`MPI_Scatter`** | $1 ightarrow M$ | Array no Root | Fatia em todos | **Sim** |
| **`MPI_Gather`** | $M ightarrow 1$ | Fatia em todos | Array no Root | **Sim** |
| **`MPI_Reduce`** | $M ightarrow 1$ | Dado local em todos | Resultado no Root | **Sim** |
| **`MPI_Allreduce`** | $M ightarrow M$ | Dado local em todos | Resultado em TODOS | **Não** |
| **`MPI_Allgather`** | $M ightarrow M$ | Fatia em todos | Array em TODOS | **Não** |

---

## 9. Exercícios de Fixação

### Questão 1
Qual é a principal vantagem de desempenho ao utilizar a rotina `MPI_Bcast` para transmitir um dado de 1 GB para 64 nós em comparação com um laço `for` simples contendo 63 chamadas sequenciais de `MPI_Send`?
* a) O `MPI_Bcast` envia os dados via memória compartilhada ignorando a placa de rede.
* b) O `MPI_Bcast` utiliza uma estrutura de árvore binária/binomial de comunicação, reduzindo a complexidade de tempo de $O(N)$ para $O(\log_2 N)$.
* c) O `MPI_Bcast` comprime o dado de 1 GB para 1 MB antes de enviar.
* d) Não há diferença de desempenho, é apenas um atalho sintático de código.

### Questão 2
Considere um cluster com 4 processos e um array de 100 inteiros no processo 0. Para distribuir 25 inteiros exclusivos para cada processo usando `MPI_Scatter`, qual deve ser o valor passado no parâmetro `sendcount`?
* a) 100
* b) 25
* c) 4
* d) 1

### Questão 3
Qual função de comunicação coletiva deve ser escolhida quando todos os processos precisam saber o valor médio global de uma variável ao final do cálculo, sem depender do processo 0 para retransmitir a informação?
* a) `MPI_Reduce` com `MPI_SUM`
* b) `MPI_Gather`
* c) `MPI_Allreduce` com `MPI_SUM` (dividido por $N$)
* d) `MPI_Bcast`

---

### Gabarito Comentado
* **Questão 1: Resposta B.** O algoritmo interno de comunicação coletiva do MPI organiza os processos em árvore, permitindo que nós que já receberam o dado ajudem a retransmiti-lo para outros nós em paralelo, alcançando escalabilidade logarítmica.
* **Questão 2: Resposta B.** O parâmetro `sendcount` no `MPI_Scatter` especifica o número de elementos enviados **para cada processo participante**, e não o total de elementos no buffer de origem.
* **Questão 3: Resposta C.** O `MPI_Allreduce` realiza a operação combinatória e entrega o resultado diretamente no buffer de recepção de **todos** os processos participantes, poupando um passo adicional de broadcast.
