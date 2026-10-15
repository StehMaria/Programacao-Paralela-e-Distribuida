# Aula 4.5: Profiling e Análise de Gargalos — gprof, perf, Intel VTune, Valgrind/Helgrind e Medição Confiável de Tempo

---

## 1. Introdução: A Regra de Ouro da Otimização ("Medir Antes de Otimizar")

Na computação de alto desempenho (HPC) e no desenvolvimento de software concorrente, existe um axioma consagrado formulado por Donald Knuth:

> *"Otimização precoce é a raiz de todo o mal."*

Muitos desenvolvedores cometem o erro grave de tentar otimizar partes do código com base em intuição ou suposições. No entanto, em sistemas modernos com arquiteturas complexas (caches multinível, execução fora de ordem, preditores de desvio e NUMA), a intuição humana frequentemente falha em adivinhar onde o programa passa a maior parte do seu tempo.

### A Analogia do Médico e do Raio-X

Imagine um médico que decide operar um paciente sem realizar exames prévios, guiando-se apenas por um palpite de onde está a dor. As chances de sucesso seriam desastrosas. 

O **Profiling** (ou perfilamento de código) é o equivalente ao **exame de Raio-X ou Ressonância Magnética** do desenvolvedor de software. Ele mapeia com precisão cirúrgica quais funções, linhas de código ou instruções Assembly estão consumindo ciclos de CPU, provocando *cache misses*, bloqueando *threads* ou travando no barramento de memória.

### O Princípio de Pareto (A Regra 80/20 em HPC)

Em quase todas as aplicações científicas e de engenharia, vale o Princípio de Pareto:
* **80% do tempo total de execução** é gasto em apenas **20% do código** (os chamados *Hotspots* ou pontos quentes).
* Otimizar um trecho de código que consome apenas 2% do tempo total de execução, mesmo que você o torne 100 vezes mais rápido, trará um ganho insignificante para o tempo global do programa (conforme demonstrado pela **Lei de Amdahl** na Aula 4.2).

O objetivo do *profiling* é identificar exatamente onde estão esses **20% críticos** para concentrar os esforços de reestruturação de laços, vetorização SIMD ou paralelização.

---

## 2. Tipos e Técnicas de Profiling

As ferramentas de *profiling* utilizam três abordagens principais para analisar a execução do programa. Cada abordagem possui um *trade-off* distinto entre precisão, detalhamento e impacto no tempo de execução (*overhead*).

```
+-------------------------------------------------------------------------+
|                         TÉCNICAS DE PROFILING                           |
+-------------------------------------------------------------------------+
| 1. SAMPLING (Aostragem por Hardware/Timer)                              |
|    - Interrompe a CPU a cada N ciclos e lê o Instruction Pointer (IP)   |
|    - Overhead Mínimo (~1% a 5%) | Ideal para Produção e HPC           |
|    - Ferramentas: Linux `perf`, Intel VTune                             |
+-------------------------------------------------------------------------+
| 2. INSTRUMENTAÇÃO (Modificação de Código/Binário)                      |
|    - Insere contadores na entrada/saída de cada função                  |
|    - Overhead Médio a Alto (10% a 100%) | Contagem exata de chamadas     |
|    - Ferramentas: GNU `gprof` (-pg)                                     |
+-------------------------------------------------------------------------+
| 3. SIMULAÇÃO DE HARDWARE (Execução em Máquina Virtual)                  |
|    - Executa o código dentro de um emulador de CPU/Memória               |
|    - Overhead Extremamente Alto (10x a 100x mais lento)                 |
|    - Análise perfeita de caches, acessos e race conditions              |
|    - Ferramentas: Valgrind (Cachegrind, Helgrind)                       |
+-------------------------------------------------------------------------+
```

### 1. Sampling (Amostragem Baseada em Timer ou Hardware)
A ferramenta instrui o sistema operacional ou a PMU (*Performance Monitoring Unit* do processador) a emitir uma interrupção periódica (ex: 1000 vezes por segundo). A cada interrupção, o profiler registra o valor do *Instruction Pointer* (IP) e a pilha de chamadas (*call stack*).
* **Vantagens:** *Overhead* irrisório (~1% a 5%). Não altera o comportamento do código nem desativa otimizações do compilador.
* **Desvantagens:** Estatístico. Pode perder funções executadas muito rapidamente entre as amostras.

### 2. Instrumentação (Instrumentation)
O compilador ou a ferramenta insere instruções extras no início e no fim de cada função ou bloco de código para contar exatamente quantas vezes a função foi chamada e quanto tempo permaneceu nela.
* **Vantagens:** Contagem exata de chamadas e mapeamento preciso do grafo de chamadas (*Call Graph*).
* **Desvantagens:** *Overhead* significativo. A inserção do código de contagem pode impedir que o compilador realize *inlining* de funções e otimizações de laços.

### 3. Simulação / Emulação de Hardware
A aplicação é executada sobre uma CPU sintética virtualizada que intercepta cada instrução de leitura, escrita, salto e alocação de memória.
* **Vantagens:** Rastreamento determinístico perfeito de *cache misses*, erros de alocação de memória e *race conditions* entre *threads*.
* **Desvantagens:** Execução extremamente lenta (de 10x a 100x mais demorada que a execução normal).

---

## 3. Identificando Gargalos: As 4 Classes Principais

Ao analisar o relatório de um profiler, os gargalos de desempenho geralmente se enquadram em quatro categorias distintas:

```
[DIAGRMA DE CLASSIFICAÇÃO DE GARGALOS]
(Local da Imagem: Indicado arquivo 'diagrama-gargalos-hpc.png')

  +-----------------------+--------------------------------------------------+
  | Categoria do Gargalo  | Sintomas Típicos no Profiler                     |
  +-----------------------+--------------------------------------------------+
  | 1. CPU-Bound          | Alta taxa de IPC (> 1.5), 100% de uso de ULA/FPU  |
  | 2. Memory-Bound       | Baixo IPC (< 0.5), alta taxa de Cache L3 Misses  |
  | 3. Sync-Bound         | Threads em estado 'Sleep/Wait', baixo uso de CPU|
  | 4. I/O-Bound          | Espera de sistema (System Time/iowait) alta      |
  +-----------------------+--------------------------------------------------+
```

1. **CPU-Bound (Gargalo de Processamento):** A CPU está operando no limite de sua capacidade de cálculo. As instruções são executadas rapidamente e o IPC (*Instructions Per Cycle*) é alto (> 1.5). **Solução:** Vetorização SIMD, reestruturação matemática, unrolling de laços.
2. **Memory-Bound (Gargalo de Memória):** A CPU passa a maior parte do tempo ociosa em estado de espera (*Stalled*) aguardando dados chegarem da RAM. O IPC é baixo (< 0.5) e há alta incidência de *L3 Cache Misses*. **Solução:** *Loop Tiling*, alteração do padrão de acesso para *Unit Stride*, estruturas SoA em vez de AoS.
3. **Synchronization-Bound (Gargalo de Sincronização):** Em código paralelo, as *threads* passam muito tempo bloqueadas esperando em *mutexes*, variáveis de condição ou barreiras. **Solução:** Redução da granularidade das travas, uso de algoritmos *lock-free*, balanceamento dinâmico de carga (`schedule(dynamic)`).
4. **I/O-Bound (Gargalo de Entrada/Saída):** A aplicação fica travada esperando leitura ou escrita em disco/rede. **Solução:** Leitura assíncrona, I/O em bloco, sistemas de arquivos paralelos (Lustre/NFS).

---

## 4. Guia Prático das Principais Ferramentas de Profiling

### 4.1. GNU Profiler (`gprof`)
O `gprof` é a ferramenta clássica de instrumentação do ecossistema GNU.

#### Passo a Passo de Uso:
1. **Compilar com a flag `-pg`:**
   ```bash
   gcc -O2 -pg -o meu_programa meu_programa.c -lm
   ```
2. **Executar normalmente:**
   ```bash
   ./meu_programa
   ```
   *Isso gerará automaticamente um arquivo de saída binário chamado `gmon.out`.*
3. **Analisar os resultados com o `gprof`:**
   ```bash
   gprof ./meu_programa gmon.out > relatorio.txt
   ```

#### Conteúdo do Relatório:
* **Flat Profile:** Mostra a porcentagem de tempo gasta em cada função e o número total de chamadas.
* **Call Graph:** Mostra quais funções chamaram quais funções e o tempo acumulado nas sub-rotinas.

---

### 4.2. Linux `perf` (A Ferramenta do Kernel Linux)
O `perf` é a ferramenta oficial de perfilamento do Linux baseada em amostragem por hardware via PMU. Ele não exige recompilação do código nem altera o binário.

#### Principais Comandos do `perf`:

1. **`perf stat` (Contadores Globais de Desempenho):**
   Fornece um resumo de alto nível da execução sem overhead.
   ```bash
   perf stat ./meu_programa
   ```
   *Exemplo de Saída:*
   ```text
       12.450,12 msec task-clock                #    3,982 CPUs utilized
           1.240      context-switches          #  100,000 /sec
              12      cpu-migrations            #    1,000 /sec
           3.412      page-faults               #  274,000 /sec
  35.120.400.000      cycles                    #    2,821 GHz
  42.144.000.000      instructions              #    1,20  insn per cycle (IPC)
     820.100.000      cache-references          #   65,870 M/sec
      84.200.000      cache-misses              #   10,26% of all L1-dcache accesses
   ```

2. **`perf record` e `perf report` (Localização de Hotspots):**
   Grava amostras detalhadas e gera o relatório interativo por linha de código.
   ```bash
   # Gravando amostras com suporte a call-graph (pilha de chamadas)
   perf record -g ./meu_programa

   # Visualizando o relatório interativo no terminal
   perf report
   ```

---

### 4.3. Intel® VTune™ Profiler (A Ferramenta Industrial)
O Intel VTune é o estado da arte em análise de desempenho para arquiteturas x86 (Intel e AMD). Ele possui interface gráfica avançada e integração com o hardware.

```
[VISUALIZAÇÃO DE FLAME GRAPH DO INTEL VTUNE]
(Local da Imagem: Indicado arquivo 'intel-vtune-flamegraph.png')
```

#### Tipos de Análise no VTune:
* **Hotspots Analysis:** Identifica as funções que consomem mais tempo de CPU.
* **Microarchitecture Exploration:** Examina contadores físicos de microarquitetura, identificando *Frontend Bound*, *Backend Bound* (Core vs. Memory Bound) e má predição de desvios (*Bad Speculation*).
* **Memory Access Analysis:** Avalia a largura de banda de memória utilizada, acessos NUMA remotos e gargalos de cache L1/L2/L3.
* **Threading Analysis:** Analisa a eficiência da paralelização, identificando *threads* ociosas (*Imbalance*) e tempo perdido em travas (*Lock & Wait*).

---

### 4.4. Valgrind Suite (`Cachegrind`, `Callgrind` e `Helgrind`)
O Valgrind é uma estrutura de simulação de binários. Para programação paralela e otimização de memória, três ferramentas do pacote destacam-se:

1. **Cachegrind (Simulador de Cache):**
   Simula a hierarquia de memória (L1i, L1d e L3) e reporta *cache misses* exatos.
   ```bash
   valgrind --tool=cachegrind ./meu_programa
   cg_annotate cachegrind.out.<pid>
   ```

2. **Callgrind (Grafo de Chamadas Detalhado):**
   Gera grafos de chamadas com contagem exata de instruções. Os resultados podem ser visualizados graficamente com o utilitário `kcachegrind`.
   ```bash
   valgrind --tool=callgrind ./meu_programa
   kcachegrind callgrind.out.<pid>
   ```

3. **Helgrind (Detector de Race Conditions e Deadlocks em Pthreads):**
   Sombra todas as operações de memória e travas para detectar automaticamente violações de concorrência.
   ```bash
   valgrind --tool=helgrind ./meu_programa_paralelo
   ```

---

## 5. Medição Confiável de Tempo em C e C++

Um aspecto crítico da análise de desempenho é saber **como medir o tempo de execução** de forma precisa e reprodutível.

### O Erro Clássico da Função `clock()`
Muitos programadores iniciantes utilizam a função `clock()` da biblioteca padrão `<time.h>` para medir o tempo de programas paralelos:

```c
// ERRO GRAVE EM CÓDIGO PARALELO!
clock_t inicio = clock();
executar_funcao_paralela_openmp();
clock_t fim = clock();
double tempo = (double)(fim - inicio) / CLOCKS_PER_SEC;
```

> **Atenção:** A função `clock()` retorna o **tempo acumulado de CPU consumido por todas as threads**. Se o seu programa rodou em 8 *threads* durante 1 segundo cronometrado no relógio de parede (*Wall-Clock Time*), o `clock()` retornará aproximadamente 8 segundos!

### Relógios Recomendados para HPC e Código Concorrente

#### 1. OpenMP: `omp_get_wtime()`
Retorna o tempo decorrido no relógio de parede em segundos com precisão de microssegundos/nanossegundos.
```c
double inicio = omp_get_wtime();
// Trecho de código paralelo
double fim = omp_get_wtime();
printf("Tempo decorrido: %f segundos\n", fim - inicio);
```

#### 2. POSIX: `clock_gettime(CLOCK_MONOTONIC, ...)`
Oferece precisão em nanostampes e é imune a alterações manuais do relógio do sistema operacional (como ajustes de fuso horário ou protocolo NTP).
```c
#include <time.h>

struct timespec inicio, fim;
clock_gettime(CLOCK_MONOTONIC, &inicio);

// Trecho a medir...

clock_gettime(CLOCK_MONOTONIC, &fim);
double tempo = (fim.tv_sec - inicio.tv_sec) + 
               (fim.tv_nsec - inicio.tv_nsec) / 1e9;
```

#### 3. C++11: `<chrono>`
A abordagem moderna e portável em C++:
```cpp
#include <chrono>

auto inicio = std::chrono::high_resolution_clock::now();
// Execução do algoritmo...
auto fim = std::chrono::high_resolution_clock::now();
std::chrono::duration<double> duracao = fim - inicio;
std::cout << "Tempo: " << duracao.count() << " s\n";
```

### Boas Práticas para Experimentos de Desempenho

1. **Aquecimento de Cache (*Warm-up*):** Execute a função uma vez antes de iniciar a medição oficial para carregar o código e as bibliotecas dinâmicas na memória e nas caches.
2. **Rodadas Múltiplas e Estatística:** Nunca confie em uma única medição. Execute o teste pelo menos 5 a 10 vezes e registre a **Média** e o **Desvio Padrão**.
3. **Desativar Gerenciamento Térmico/Frequência Dinâmica:** Em servidores de benchmarking, desative o *Intel Turbo Boost* / *AMD Core Performance Boost* e fixe a frequência da CPU para evitar variações provocadas por aquecimento térmico.

---

## 6. Exemplo Prático Completo em C: Diagnóstico de Hotspots e Medição de Alta Precisão

O código a seguir simula uma aplicação com dois módulos distintos: um módulo **CPU-Bound** (cálculos trigonométricos) e um módulo **Memory-Bound** (percurso inadequado de matriz). Ele inclui medição de tempo de alta precisão imune ao número de *threads*.

```c
/*
 * aula_4_5_profiling_demo.c
 * Demonstração de perfilamento e medição confiável de tempo.
 * Compilação recomendada:
 *   gcc -O2 -fopenmp -g aula_4_5_profiling_demo.c -o profiling_demo -lm
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <omp.h>

#define N 4000

// Matriz global para evitar alocação na pilha
double A[N][N];

// Função 1: Módulo CPU-Bound (Operações aritméticas intensivas)
void modulo_cpu_bound() {
    #pragma omp parallel for collapse(2)
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            A[i][j] = sin(i) * cos(j) + sqrt(i + j + 1.0);
        }
    }
}

// Função 2: Módulo Memory-Bound (Acesso por coluna - Stride-N inadequado)
void modulo_memory_bound() {
    #pragma omp parallel for
    for (int j = 0; j < N; j++) {
        for (int i = 0; i < N; i++) {
            // Acesso saltado por coluna que quebra a cache L1/L2!
            A[i][j] += 1.0;
        }
    }
}

int main() {
    printf("=== DEMONSTRAÇÃO DE PROFILING E MEDIÇÃO DE DESEMPENHO ===\n");
    printf("Tamanho da Matriz: %d x %d\n", N, N);
    printf("Threads OpenMP ativas: %d\n\n", omp_get_max_threads());

    // 1. Aquecimento de Cache (Warm-up)
    modulo_cpu_bound();

    // 2. Medição do Módulo CPU-Bound
    double t_inicio_cpu = omp_get_wtime();
    modulo_cpu_bound();
    double t_fim_cpu = omp_get_wtime();
    double tempo_cpu = t_fim_cpu - t_inicio_cpu;

    // 3. Medição do Módulo Memory-Bound
    double t_inicio_mem = omp_get_wtime();
    modulo_memory_bound();
    double t_fim_mem = omp_get_wtime();
    double tempo_mem = t_fim_mem - t_inicio_mem;

    // Relatório de Tempos no Relógio de Parede
    printf("--- RESULTADOS DA MEDIÇÃO (Wall-Clock Time) ---\n");
    printf("Tempo Módulo CPU-Bound:    %.4f segundos\n", tempo_cpu);
    printf("Tempo Módulo Memory-Bound: %.4f segundos\n", tempo_mem);
    printf("Tempo Total de Execução:   %.4f segundos\n", tempo_cpu + tempo_mem);

    return 0;
}
```

### Roteiro de Experimento no Terminal:

```bash
# 1. Compilar o programa com símbolos de debug (-g) para o profiler
gcc -O2 -fopenmp -g aula_4_5_profiling_demo.c -o profiling_demo -lm

# 2. Executar o perf stat para analisar os contadores globais de hardware
perf stat ./profiling_demo

# 3. Registrar amostras com o perf record para descobrir o hotspot exato
perf record -g ./profiling_demo

# 4. Inspecionar o relatório interativo
perf report
```

---

## 7. Tabela Comparativa das Ferramentas de Profiling

```
+---------------+-------------------+-----------------+-----------------------+-----------------------------+
| Ferramenta    | Método            | Overhead        | Requer Recompilação?  | Melhor Caso de Uso          |
+---------------+-------------------+-----------------+-----------------------+-----------------------------+
| gprof         | Instrumentação    | Médio (~10-30%) | Sim (`-pg`)           | Grafo de chamadas simples   |
| Linux perf    | Sampling (PMU)    | Mínimo (< 2%)   | Não                   | Análise em Produção / IPC   |
| Intel VTune   | Sampling Avançado | Baixo (~2-5%)   | Não                   | Análise Microarquitetural   |
| Cachegrind    | Emulação          | Alto (10x-50x)  | Não                   | Detecção exata Cache Misses |
| Helgrind      | Emulação          | Alto (20x-100x) | Não                   | Race conditions / Pthreads  |
+---------------+-------------------+-----------------+-----------------------+-----------------------------+
```

---

## 8. Síntese de Fechamento do Módulo 4 & Exercícios de Fixação

Com esta aula, concluímos formalmente o **Módulo 4 (Análise de Desempenho, Profiling e Modelagem)**. 

### Visão Integrada do Módulo 4:
1. **Métricas de Desempenho (Aula 4.1):** Aprendemos a calcular *Wall-Clock Time*, *Speedup*, *Eficiência* e distinguir *Strong* e *Weak Scaling*.
2. **Lei de Amdahl (Aula 4.2):** Entendemos o limite assintótico imposto pela fração sequencial crítica (\(f\)) em *Strong Scaling*.
3. **Lei de Gustafson (Aula 4.3):** Descobrimos como superar o limite de Amdahl expandindo o tamanho do problema proporcionalmente ao número de processadores (*Weak Scaling*).
4. **Modelo PRAM (Aula 4.4):** Estudamos a abstração teórica de algoritmos paralelos síncronos (EREW, CREW, CRCW) e o Teorema de Brent.
5. **Profiling e Análise de Gargalos (Aula 4.5):** Adquirimos o arsenal prático para diagnosticar e medir gargalos no código real com medição confiável e ferramentas do ecossistema Linux.

---

### Exercícios de Fixação

#### Questão 1
Por que a função `clock()` da linguagem C não deve ser utilizada para medir o *speedup* de um algoritmo paralelo paralelizado com OpenMP ou Pthreads?
* A) Porque a função `clock()` tem precisão apenas em minutos.
* B) Porque ela mede o tempo acumulado de CPU de todas as *threads*, mascarando o tempo real decorrido no relógio de parede (*Wall-Clock Time*).
* C) Porque o compilador remove chamadas para `clock()` durante otimizações `-O3`.
* D) Porque o `clock()` causa bloqueios de sincronização entre as *threads*.

#### Questão 2
Um desenvolvedor rodou a ferramenta `perf stat` em seu código e obteve um IPC (*Instructions Per Cycle*) de **0.25** e uma taxa de *L3 Cache Misses* de **18%**. Qual é a classificação primária do gargalo deste programa e qual estratégia deve ser adotada?
* A) Gargalo CPU-Bound; aplicar vetorização AVX-512.
* B) Gargalo de Sincronização; trocar *mutexes* por *spinlocks*.
* C) Gargalo Memory-Bound; aplicar reestruturação de laços (*Loop Tiling/Interchange*) para melhorar a localidade espacial.
* D) Gargalo de I/O; utilizar escrita em buffer assíncrono.

#### Questão 3
Qual das seguintes ferramentas do Valgrind é especificamente projetada para detectar *Race Conditions* e potenciais *Deadlocks* em aplicações que utilizam a biblioteca POSIX Threads (Pthreads)?
* A) Cachegrind
* B) Callgrind
* C) Massif
* D) Helgrind

---

### Gabarito Comentado

* **Questão 1:** **Resposta B**. O `clock()` contabiliza o tempo de CPU somado em cada núcleo ativo. Se 8 *threads* rodarem 100% ativas durante 1 segundo real, `clock()` reportará 8 segundos de processamento, impedindo o cálculo correto do *Speedup*. O correto é usar `omp_get_wtime()`, `clock_gettime(CLOCK_MONOTONIC)` ou `<chrono>`.
* **Questão 2:** **Resposta C**. Um IPC baixo (< 0.5) acompanhado de alta taxa de *Cache Misses* é o sintoma clássico de um programa **Memory-Bound**, onde a CPU fica bloqueada esperando dados da memória principal. A solução adequada envolve otimizar o padrão de acesso à memória (*Unit Stride* / *Tiling*).
* **Questão 3:** **Resposta D**. O **Helgrind** é a ferramenta do pacote Valgrind especializada em monitorar acessos concorrentes à memória e operações com travas em Pthreads para sinalizar condições de corrida e violações da ordem de bloqueio.
