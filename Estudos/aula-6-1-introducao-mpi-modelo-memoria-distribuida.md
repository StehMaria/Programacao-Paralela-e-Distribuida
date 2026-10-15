# Aula 6.1: Introdução ao MPI e ao Modelo de Memória Distribuída — Ambiente, Comunicadores e Ciclo de Vida Básicos

---

## 1. Do Escritório Único às Filiais Distribuídas: A Transição para Memória Distribuída

Até o módulo anterior (OpenMP), trabalhamos sob o paradigma de **Memória Compartilhada** (*Shared Memory*). Imagine uma equipe de programadores sentada ao redor de uma mesma mesa de reunião: todos conseguem olhar para o mesmo quadro branco e escrever nas mesmas folhas de papel. Se a *Thread 0* altera uma variável, a *Thread 1* enxerga a mudança instantaneamente na memória RAM compartilhada.

Contudo, computadores com memória compartilhada possuem um limite físico de expansão (*scalability wall*). Placas-mãe de grande porte com dezenas de soquetes de CPU tornam-se exponencialmente caras e complexas devido ao tráfego de coerência de cache.

Para resolver problemas gigantescos — como previsão do tempo em escala planetária, simulação de fusão nuclear ou treinamento de grandes modelos de IA —, precisamos conectar centenas ou milhares de computadores independentes através de uma rede de altíssima velocidade (InfiniBand, Ethernet de 100 Gbps). Isso é um **Cluster de Computadores** ou **Sistema de Memória Distribuída**.

```
[Memória Compartilhada (OpenMP)]
+-------------------------------------------------------+
|  CPU Core 0   CPU Core 1   CPU Core 2   CPU Core 3    |
|       |            |            |            |        |
|  +-------------------------------------------------+  |
|  |           Memória RAM Única Compartilhada        |  |
|  +-------------------------------------------------+  |
+-------------------------------------------------------+

[Memória Distribuída (MPI)]
+-----------------+    Rede de Alta    +-----------------+
|  Nó 0 (PC 0)    |    Velocidade      |  Nó 1 (PC 1)    |
|  +-----------+  |   (InfiniBand)     |  +-----------+  |
|  | CPU | RAM |  |<==================>|  | CPU | RAM |  |
|  +-----------+  |                    |  +-----------+  |
+-----------------+                    +-----------------+
```

> 💡 **Analogia da Empresa Multinacional:**
> - **OpenMP (Memória Compartilhada):** É como funcionários em uma única sala trocando anotações em um mesmo quadro. A comunicação é instantânea, mas a sala tem capacidade limitada.
> - **MPI (Memória Distribuída):** É como escritórios em cidades diferentes (São Paulo, Lisboa, Tóquio). Não existe quadro branco compartilhado. Se o escritório de São Paulo precisa que o de Lisboa processe um dado, precisa **enviar uma carta/e-mail (mensagem)** com o dado pelo correio (rede).

---

## 2. O Padrão MPI (*Message Passing Interface*) e o Paradigma SPMD

O **MPI (Message Passing Interface)** não é uma linguagem de programação nem um software específico, mas sim um **padrão internacional padronizado** (*standard*) mantido pelo *Message Passing Interface Forum* (MPI Forum). Ele define uma API rica para troca de mensagens entre processos isolados em C, C++ e Fortran.

### Mitos e Fatos sobre o MPI:
1. **É uma especificação, não uma biblioteca única:** Existem várias implementações do padrão, como **OpenMPI**, **MPICH**, **MVAPICH** e **Intel MPI**. O seu código C compilado com o padrão MPI funciona em qualquer uma delas.
2. **Processos, não Threads:** No MPI, a unidade fundamental de execução é o **processo do sistema operacional**. Cada processo possui seu próprio espaço de endereçamento privado, sua própria tabela de páginas e suas próprias variáveis. Não há risco de *Data Race* em memória RAM, pois um processo **não consegue ler a memória do outro diretamente**.
3. **Pode rodar tanto em Cluster quanto em uma única máquina:** Você pode rodar um programa MPI com 8 processos no seu notebook de 8 núcleos para testes, ou lançá-lo em 10.000 nós de um supercomputador sem alterar uma única linha de código.

### O Paradigma SPMD (*Single Program, Multiple Data*)
A forma mais comum de programar com MPI é o modelo **SPMD**:
- Um **único arquivo executável** compilado é copiado e distribuído para todos os nós do cluster.
- Várias instâncias idênticas desse programa são inicializadas simultaneamente.
- Cada processo descobre o seu identificador numérico único (**Rank**).
- Com base nesse **Rank**, o programa decide qual fatia do trabalho cada nó vai executar.

```
                  +--------------------------+
                  |  Código Único (binary)   |
                  +--------------------------+
                               |
            +------------------+------------------+
            |                                     |
            v                                     v
+-----------------------+             +-----------------------+
|  Processo 0 (Rank 0)  |             |  Processo 1 (Rank 1)  |
| "Se rank == 0:        |             | "Se rank == 0:        |
|  Sou o Mestre, leia   |             |  Sou o Mestre..."     |
|  o arquivo de entrada"|             | "Se rank == 1:        |
|                       |             |  Processa bloco B"    |
+-----------------------+             +-----------------------+
```

---

## 3. Ambiente e Comunicadores: O Conceito de `MPI_COMM_WORLD`

Para que um processo possa enviar mensagens a outro, o sistema de comunicação precisa saber **quem faz parte da conversa**. Esse grupo de processos organizados e isolados é chamado de **Comunicador** (*Communicator*).

```
   ===========================================================
   |                   MPI_COMM_WORLD                        |
   |                                                         |
   |  +------------+   +------------+   +------------+   |
   |  | Processo 0 |   | Processo 1 |   | Processo 2 |   |
   |  |  (Rank 0)  |   |  (Rank 1)  |   |  (Rank 2)  |   |
   |  +------------+   +------------+   +------------+   |
   ===========================================================
```

### O Comunicador Global: `MPI_COMM_WORLD`
Quando um programa MPI é lançado, a biblioteca cria automaticamente um comunicador predefinido contendo todos os processos iniciados na execução. Ele é denominado **`MPI_COMM_WORLD`**.

Dentro de um comunicador, existem duas propriedades fundamentais:

1. **`size` (Tamanho):** Quantos processos existem no total dentro do comunicador. É uma constante durante toda a execução.
2. **`rank` (Identificador):** O número de ordem de cada processo, variando de $0$ até $	ext{size} - 1$.
   - O **Rank 0** por convenção é frequentemente escolhido para ser o processo *Mestre* ou *Coordenador* (responsável por I/O, leitura de arquivos e exibição de resultados no terminal).

> 🖼️ *Indicação de Imagem dos Slides:* Para visualizar diagramas de comunicadores e topologias do MPI, consulte o arquivo `14_mpi.pdf` ou `HPC-mpi.pdf` presente nas suas fontes.

---

## 4. O Ciclo de Vida de um Programa MPI: As Rotinas Fundamentais

Todo programa MPI escrito em C precisa importar o cabeçalho `<mpi.h>` e respeitar um ciclo de vida estrito demarcado pelas rotinas de inicialização e finalização.

```
       +------------------------------------+
       |          Início do Main            |
       +------------------------------------+
                         |
                         v
       +------------------------------------+
       |          MPI_Init(&argc, &argv)    |
       |  (Inicializa canal de rede/MPI)    |
       +------------------------------------+
                         |
                         v
       +------------------------------------+
       |  MPI_Comm_rank / MPI_Comm_size     |
       |  (Descobre ID e Total de Nós)      |
       +------------------------------------+
                         |
                         v
       +------------------------------------+
       |    Trabalho Paralelo e Troca de    |
       |       Mensagens (Send/Recv)        |
       +------------------------------------+
                         |
                         v
       +------------------------------------+
       |            MPI_Finalize()          |
       |  (Encerra subsistema de mensagens) |
       +------------------------------------+
                         |
                         v
       +------------------------------------+
       |           Fim do Main              |
       +------------------------------------+
```

### 1. `MPI_Init(int *argc, char ***argv)`
- **O que faz:** Inicializa o subsistema de comunicação MPI, estabelece as conexões de rede entre os nós do cluster e aloca as estruturas internas.
- **Regra:** Deve ser chamada **antes de qualquer outra rotina MPI**. Nenhuma função do MPI funcionará antes de `MPI_Init`.

### 2. `MPI_Comm_size(MPI_Comm comm, int *size)`
- **O que faz:** Preenche a variável apontada por `size` com o número total de processos ativos no comunicador informado (normalmente `MPI_COMM_WORLD`).

### 3. `MPI_Comm_rank(MPI_Comm comm, int *rank)`
- **O que faz:** Preenche a variável apontada por `rank` com o identificador exclusivo (de $0$ a $	ext{size}-1$) do processo que fez a chamada.

### 4. `MPI_Get_processor_name(char *name, int *resultlen)`
- **O que faz:** Retorna uma string com o nome da máquina física (*hostname*) onde aquele processo específico está rodando. Muito útil para confirmar se o cluster está de fato distribuindo os processos em nós diferentes.

### 5. `MPI_Finalize()`
- **O que faz:** Desativa a infraestrutura do MPI, libera conexões de socket/InfiniBand e garante que todas as mensagens pendentes foram entregues.
- **Regra:** Nenhuma rotina MPI pode ser chamada após `MPI_Finalize()`. É a última instrução válida do MPI antes do encerramento do programa.

---

## 5. Exemplo Prático Completo em C: "Hello World" Distribuído

Abaixo está o código C completo e comentado. Ele demonstra a captura de `rank`, `size`, o nome do host e uma divisão lógica de papéis entre o processo Mestre (`rank == 0`) e os trabalhadores (`rank > 0`).

```c
#include <stdio.h>
#include <stdlib.h>
#include <mpi.h> // Cabeçalho obrigatório do MPI

int main(int argc, char *argv[]) {
    int rank, size, name_len;
    char processor_name[MPI_MAX_PROCESSOR_NAME];

    // 1. Inicializa o ambiente MPI
    MPI_Init(&argc, &argv);

    // 2. Descobre a quantidade total de processos lançados
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    // 3. Descobre o ID (Rank) do processo atual
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    // 4. Descobre o nome da máquina física (hostname)
    MPI_Get_processor_name(processor_name, &name_len);

    // Mensagem executada por TODOS os processos (Modelo SPMD)
    printf("--> Processo %d de %d iniciado na máquina: %s
", 
           rank, size, processor_name);

    // 5. Divisão lógica de papéis baseada no Rank
    if (rank == 0) {
        printf("
======================================================
");
        printf("[MESTRE] Eu sou o Processo 0!
");
        printf("[MESTRE] Total de nós na rede de trabalho: %d
", size);
        printf("[MESTRE] Vou coordenar a execução das tarefas.
");
        printf("======================================================

");
    } else {
        printf("[TRABALHADOR] Processo %d pronto para receber instruções!
", rank);
    }

    // 6. Encerra o ambiente MPI obrigatoriamente
    MPI_Finalize();

    return 0;
}
```

### Como Compilar e Executar no Terminal Linux

Diferente de programas C comuns que usam `gcc`, programas MPI usam um **wrapper de compilação** que injeta os caminhos de biblioteca e arquivos de cabeçalho necessários automaticamente:

```bash
# Compilando o programa C usando o wrapper mpicc
mpicc -O2 ola_mpi.c -o ola_mpi

# Executando localmente simulando 4 processos
mpirun -np 4 ./ola_mpi
```

#### Exemplo de Saída Esperada no Terminal:
```text
--> Processo 0 de 4 iniciado na máquina: node01
--> Processo 1 de 4 iniciado na máquina: node01
--> Processo 2 de 4 iniciado na máquina: node02
--> Processo 3 de 4 iniciado na máquina: node02

======================================================
[MESTRE] Eu sou o Processo 0!
[MESTRE] Total de nós na rede de trabalho: 4
[MESTRE] Vou coordenar a execução das tarefas.
======================================================

[TRABALHADOR] Processo 1 pronto para receber instruções!
[TRABALHADOR] Processo 2 pronto para receber instruções!
[TRABALHADOR] Processo 3 pronto para receber instruções!
```

---

## 6. Tabela Comparativa: OpenMP vs. MPI

| Característica | OpenMP | MPI (*Message Passing Interface*) |
| :--- | :--- | :--- |
| **Modelo de Memória** | Compartilhada (*Shared Memory*) | Distribuída (*Distributed Memory*) |
| **Unidade de Execução** | *Threads* (dentro do mesmo processo) | *Processos* (isolados via SO) |
| **Hardware Alvo** | Servidores e PCs Multicore (1 nó) | Clusters, Supercomputadores e PCs Nacionais |
| **Comunicação** | Implicita (leitura/escrita em RAM) | Explícita (Envio/Recebimento de Mensagens) |
| **Escalabilidade** | Limitada pela memória física do servidor | Praticamente ilimitada (milhares de nós) |
| **Início do Código** | Diretivas `#pragma omp` em C | `MPI_Init()` e ciclo de vida explícito |
| **Variáveis Globais** | Compartilhadas por padrão | Privadas de cada processo |

---

## 7. Exercícios de Fixação

### Questão 1
Por que uma variável global declarada no início de um código C com MPI não compartilha seu valor alterado com os demais processos do cluster durante a execução?
- **A)** Porque o compilador `mpicc` desativa variáveis globais.
- **B)** Porque o MPI roda em threads que possuem registradores protegidos.
- **C)** Porque cada processo MPI possui seu próprio espaço de endereçamento privado e memória RAM isolada.
- **D)** Porque a função `MPI_Init` apaga a memória RAM no início da execução.

### Questão 2
O que acontece se um desenvolvedor chamar uma função como `MPI_Comm_rank` antes de chamar `MPI_Init`?
- **A)** O programa executa normalmente assumindo rank 0.
- **B)** Ocorre um erro de tempo de execução (*runtime crash* ou comportamento indefinido) pois a infraestrutura do MPI ainda não foi inicializada.
- **C)** O programa compila automaticamente como OpenMP.
- **D)** O sistema operacional cria uma thread privada de emergência.

### Questão 3
Qual a função correta do identificador `MPI_COMM_WORLD`?
- **A)** É uma função que calcula o tempo global de execução em segundos.
- **B)** É o comunicador padrão que agrupa todos os processos MPI iniciados no lançamento do programa.
- **C)** É o nome da máquina mestre no cluster.
- **D)** É o arquivo de log onde as mensagens são salvas.

---

### Gabarito Comentado

1. **Resposta C:** No modelo de memória distribuída do MPI, cada instância é um processo do sistema operacional com espaço de memória 100% isolado. Alterar uma variável no nó 0 não afeta o nó 1 a menos que uma mensagem explícita seja enviada.
2. **Resposta B:** `MPI_Init` é a rotina obrigatória de bootstrapped da infraestrutura. Tentar chamar funções de comunicação antes de inicializar o subsistema resulta em falhas críticas ou erros de link de runtime.
3. **Resposta B:** `MPI_COMM_WORLD` é o comunicador pré-definido global que engloba todos os processos iniciados pelo comando `mpirun`/`mpiexec`.
