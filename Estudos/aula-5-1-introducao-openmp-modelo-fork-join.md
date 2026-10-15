# Aula 5.1: Introdução ao OpenMP, Modelo Fork-Join e Arquitetura de Diretivas (`#pragma omp parallel`)

---

## 1. O que é o OpenMP?

No universo da programação concorrente e paralela em sistemas de **memória compartilhada** (como computadores multicore modernos, servidores SMP e nós NUMA), o **OpenMP** (*Open Multi-Processing*) é o padrão da indústria para o desenvolvimento de aplicações paralelas de alto desempenho em C, C++ e Fortran.

### O Grande Problema do Pthreads (E por que o OpenMP surgiu)
Programar diretamente com APIs de baixo nível como **POSIX Threads (Pthreads)** exige que o desenvolvedor gerencie manualmente a criação de threads, a passagem de parâmetros, a sincronização fina por mutexes, a destruição de threads e o tratamento de ponteiros `void*`. Isso torna o código verboso, propenso a erros sutis (*deadlocks*, vazamento de memória de stack, *race conditions*) e de difícil manutenção.

O OpenMP adota uma abordagem radicalmente diferente: **programação declarativa guiada por diretivas de compilador**. Em vez de reescrever a arquitetura do programa, você pega um código sequencial correto e adiciona "anotações" (*pragmas*) indicando ao compilador quais regiões devem ser executadas em paralelo.

```
+-----------------------------------------------------------------------+
|                            SUA APLICAÇÃO                              |
+-----------------------------------------------------------------------+
|  Diretivas de Compilador (#pragma omp ...)                            |
|  Rotinas de Biblioteca (<omp.h>)                                     |
|  Variáveis de Ambiente (OMP_NUM_THREADS)                             |
+-----------------------------------------------------------------------+
|                  COMPILADOR (GCC, Clang, Intel ICX)                   |
|          (Traduz pragmas em código multithread runtime)               |
+-----------------------------------------------------------------------+
|                       SISTEMA OPERACIONAL (Kernel)                    |
|                        (Escalonamento de Threads)                     |
+-----------------------------------------------------------------------+
```

### Analogia: O Maestro e a Orquestra

> Imagine um **Maestro** (a *Master Thread* ou Thread 0) no palco de um teatro. Na maior parte do tempo, o maestro lê a partitura sequencialmente. Quando a partitura exige um acorde monumental, o maestro faz um gesto com a batuta (*Fork*). Instantaneamente, **uma equipe completa de músicos** (*Team of Threads*) entra em ação, tocando suas respectivas notas simultaneamente. Assim que a passagem complexa termina, o maestro abaixa a batuta (*Join* / Barreira) e os músicos silenciam, deixando o maestro seguir sozinho até o próximo momento coletivo.

---

## 2. O Modelo de Execução Fork-Join

A espinha dorsal do OpenMP é o modelo de execução **Fork-Join**.

1. **Início Sequencial (Master Thread):** O programa inicia a execução com uma única thread ativa, chamada de **Master Thread** (ID = 0).
2. **Fork (Divisão):** Quando a Master Thread encontra uma diretiva paralela (`#pragma omp parallel`), ela cria ou desperta do *pool* um time de threads secundárias (*slave/worker threads*).
3. **Região Paralela:** O bloco de código associado à diretiva paralela é executado **simultaneamente por todas as threads da equipe**.
4. **Join (Junção):** No final da região paralela, existe uma **barreira implícita de sincronização**. Nenhuma thread pode prosseguir até que *todas* as threads da equipe terminem sua parte. As threads secundárias voltam para o estado suspenso/dormindo (*pool*), e a Master Thread continua a execução sequencial do programa.

```
Tempo
  |
  |       [ Thread 0 (Master Thread) ]  (Execução Sequencial)
  |                   |
  |                   v
  |            =================  <--- FORK (#pragma omp parallel)
  |           /   |       |     \
  |          /    |       |      \
  |         v     v       v       v
  |      Thread0 Thread1 Thread2 Thread3  (Execução Paralela)
  |         |     |       |       |
  |         |     |       |       |
  |         v     v       v       v
  |            =================  <--- JOIN (Barreira Implícita)
  |                   |
  |                   v
  |       [ Thread 0 (Master Thread) ]  (Execução Sequencial)
  v
```

> **Indicação de Imagem:** `[INSERIR IMAGEM: Diagrama ilustrativo do Modelo Fork-Join mostrando a Thread Master se dividindo em um grupo de threads concorrentes dentro de uma caixa demarcada 'Parallel Region' e se reunindo em um ponto de Join no final].`

---

## 3. Arquitetura de Diretivas e Estrutura do Código em C/C++

No C e C++, as diretivas do OpenMP são expressas usando a diretiva do pré-processador `#pragma omp`.

### Sintaxe Geral
```c
#pragma omp <nome-da-diretiva> [cláusula1, cláusula2, ...]
{
    /* Bloco Estruturado de Código (Structured Block) */
}
```

### O Conceito de Bloco Estruturado (*Structured Block*)
O bloco de código governado por uma diretiva paralela **deve** ser um *Structured Block*. Isso significa que o bloco tem **exatamente um ponto de entrada no topo e exatamente um ponto de saída na base**.

*   ❌ **Proibido:** Usar `goto`, `break`, `continue` ou `return` que saltem para fora da região paralela.
*   ❌ **Proibido:** Dar um `goto` para dentro da região paralela vindo de fora.
*   ✅ **Permitido:** Chamadas de função dentro do bloco, desde que a função retorne normalmente.
*   ✅ **Permitido:** Comando `exit()` para encerrar toda a aplicação imediatamente.

### Compilação e Suporte Transparente
Para compilar programas com suporte a OpenMP, é necessário passar a flag apropriada ao compilador:

*   **GCC / Clang:** `gcc -fopenmp meu_programa.c -o meu_programa`
*   **Intel OneAPI (icx):** `icx -qopenmp meu_programa.c -o meu_programa`
*   **MSVC (Windows):** `cl /openmp meu_programa.c`

> **Vantagem das Diretivas:** Se você compilar o código *sem* a flag `-fopenmp`, o compilador simplesmente ignora as linhas `#pragma omp`. O programa continuará funcionando perfeitamente como um código sequencial tradicional, sem erros de sintaxe!

---

## 4. Rotinas Principais da Biblioteca (`<omp.h>`)

Além das diretivas `#pragma`, o OpenMP fornece uma API de funções declaradas no arquivo de cabeçalho `<omp.h>`.

| Função | Tipo de Retorno | Descrição |
| :--- | :--- | :--- |
| `omp_get_thread_num()` | `int` | Retorna o ID único da thread atual dentro da equipe (de `0` até `N-1`). |
| `omp_get_num_threads()` | `int` | Retorna o número total de threads ativas na equipe atual. |
| `omp_set_num_threads(int n)`| `void` | Define o número padrão de threads para as próximas regiões paralelas. |
| `omp_get_max_threads()` | `int` | Retorna o limite máximo de threads configurado para o sistema. |
| `omp_get_wtime()` | `double` | Retorna o tempo de relógio decorrido (em segundos) desde um ponto fixo no passado. |

---

## 5. Variáveis de Ambiente de Controle

O comportamento do tempo de execução (*runtime*) do OpenMP pode ser controlado externamente via terminal, sem necessidade de alterar ou recompilar o código C.

1. **`OMP_NUM_THREADS`:** Define o número de threads que serão criadas no próximo *Fork*.
   ```bash
   export OMP_NUM_THREADS=8
   ./meu_programa
   ```
2. **`OMP_DISPLAY_ENV`:** Quando configurada como `TRUE`, o runtime imprime na tela todas as configurações ativas do OpenMP antes de executar o programa.
   ```bash
   export OMP_DISPLAY_ENV=TRUE
   ./meu_programa
   ```

---

## 6. Exemplo Prático Completo em C

Abaixo está o exemplo clássico "Hello World Parallel" detalhado, demonstrando a inicialização do ambiente, a captura de IDs e a medição de tempo de execução com `omp_get_wtime()`.

```c
#include <stdio.h>
#include <stdlib.h>
#include <omp.h>

int main(int argc, char *argv[]) {
    double inicio, fim;

    // 1. Marca o tempo inicial de execução
    inicio = omp_get_wtime();

    printf("=== Inicio da Execucao Sequencial (Thread Master) ===\n");
    printf("Total de processadores disponiveis no sistema: %d\n", omp_get_num_procs());
    printf("Maximo de threads configuradas no ambiente: %d\n\n", omp_get_max_threads());

    // 2. Início da Região Paralela (FORK)
    // Criará uma equipe de threads baseada na variável OMP_NUM_THREADS
    #pragma omp parallel
    {
        // Variáveis declaradas dentro do bloco são PRIVADAS para cada thread
        int thread_id = omp_get_thread_num();
        int total_threads = omp_get_num_threads();

        // Impressão paralela (A ordem de saída no console pode variar!)
        printf("Ola Mundo! Eu sou a thread %d de um total de %d threads.\n", 
               thread_id, total_threads);

        // Apenas a thread master executa este bloco condicional
        if (thread_id == 0) {
            printf("--> [Thread Master] Confirmando que o time paralelo esta ativo.\n");
        }
    } // <--- JOIN (Barreira implícita de sincronização)

    printf("\n=== Fim da Regiao Paralela (Retorno ao modo Sequencial) ===\n");

    // 3. Marca o tempo final e calcula a duração
    fim = omp_get_wtime();
    printf("Tempo total de execucao: %.6f segundos.\n", fim - inicio);

    return 0;
}
```

### Instruções de Compilação e Execução

```bash
# Compilação no Linux com GCC
gcc -fopenmp -O2 hello_openmp.c -o hello_openmp

# Execução testando diferentes quantidades de threads via ambiente
export OMP_NUM_THREADS=4
./hello_openmp
```

---

## 7. Exercícios de Fixação com Gabarito Comentado

### Questão 1
**O que acontece se uma thread dentro de uma região `#pragma omp parallel` executar o comando `return 0;`?**
* a) Apenas a thread que executou o `return` finaliza, e as outras continuam normalmente.
* b) O programa gera um erro de compilação ou comportamento indefinido no runtime, pois viola a regra do *Structured Block*.
* c) O compilador converte automaticamente o `return` em um `omp_barrier`.
* d) A thread master assume a execução da thread encerrada.

> **Gabarito:** **b) O programa gera um erro de compilação ou comportamento indefinido no runtime.**
> *Explicação:* As regiões paralelas do OpenMP exigem blocos estruturados (*Structured Blocks*) com entrada e saída únicas. Saltos para fora do bloco com `return`, `goto` ou `break` não são permitidos pela especificação do OpenMP.

### Questão 2
**Se a variável de ambiente `OMP_NUM_THREADS` não for definida pelo usuário no terminal, quantas threads o OpenMP criará por padrão na diretiva `#pragma omp parallel`?**
* a) Exatamente 1 thread.
* b) Exatamente 2 threads.
* c) Um número de threads igual ao número de núcleos/threads lógicos de hardware disponíveis no sistema.
* d) Nenhuma thread (o programa encerra com falha).

> **Gabarito:** **c) Um número de threads igual ao número de núcleos/threads lógicos de hardware disponíveis no sistema.**
> *Explicação:* Por padrão, a implementação padrão das bibliotecas OpenMP (como `libgomp` no GCC) inspeciona o hardware e aloca um número de threads igual ao total de processadores lógicos identificados pelo sistema operacional.

---

## Resumo da Aula

* O **OpenMP** simplifica drasticamente a programação paralela em memória compartilhada ao substituir chamadas manuais de threads por **diretivas de compilador**.
* O **modelo Fork-Join** altera continuamente o estado do programa entre execução sequencial (Master Thread) e execução paralela por uma equipe de threads (*Team of Threads*).
* Toda região paralela exige um **bloco estruturado** com ponto único de entrada e saída, delimitado por uma **barreira implícita** de sincronização no *Join*.
* O comportamento do tempo de execução pode ser facilmente inspecionado e controlado via rotinas da biblioteca `<omp.h>` e variáveis de ambiente como `OMP_NUM_THREADS`.
