# Aula 4.1: Métricas de Desempenho — Tempo de Execução, Speedup, Eficiência e Escalabilidade

---

## 🎯 Objetivos de Aprendizagem

Ao final desta aula, você será capaz de:
1. Compreender a diferença crítica entre **Wall-Clock Time** (tempo de relógio) e **CPU Time** (tempo acumulado de processador), sabendo escolher as funções de cronometragem adequadas em C/C++, OpenMP e MPI.
2. Calcular e interpretar formalmente a **Aceleração (*Speedup*)** e a **Eficiência (*Efficiency*)** de uma aplicação paralela.
3. Explicar o fenômeno raro do **Speedup Superlinear** e identificar suas causas físicas na arquitetura de memória cache.
4. Diferenciar com clareza os conceitos de **Escalabilidade Forte (*Strong Scaling*)** e **Escalabilidade Fraca (*Weak Scaling*)**.
5. Implementar medições rigorosas de tempo em código paralelo em C/OpenMP e gerar relatórios de desempenho autônomos.

---

## 1. Introdução: Por que Medir o Desempenho?

> *"O que não se pode medir, não se pode gerenciar — e muito menos otimizar."* — Adaptado da célebre máxima da engenharia.

Na **Computação de Alto Desempenho (HPC)**, reescrever um código sequencial para rodar em paralelo usando Pthreads, OpenMP, MPI ou CUDA dá um trabalho considerável. Adicionamos mutexes, gerenciamos concorrência, evitamos condições de corrida e reestruturamos laços.

Mas como sabemos se todo esse esforço valeu a pena?
* O programa ficou realmente mais rápido?
* O uso de 16 núcleos de processamento reduziu o tempo de execução para $\frac{1}{16}$ do tempo original?
* Onde o sistema está perdendo tempo: na computação útil ou no gasto de gerenciar threads e trocar mensagens?

Para responder a essas perguntas com rigor científico, precisamos de **Métricas de Desempenho**. Nesta aula, iniciaremos o **Módulo 4 (Análise de Desempenho, Profiling e Modelagem)** aprendendo as fundações conceituais e matemáticas para avaliar o ganho de desempenho de aplicações paralelas.

---

## 2. Tempo de Execução: Wall-Clock Time vs. CPU Time

A métrica mais fundamental de qualquer sistema computacional é o **Tempo de Execução ($T$)**. No entanto, quando entramos no mundo paralelo, a palavra "tempo" ganha duas interpretações completamente distintas.

```
+-----------------------------------------------------------------------+
|                         LINHA DO TEMPO REAL                           |
|  10:00:00 AM                                             10:00:05 AM  |
|  [Início]--------------------------------------------------->[Fim]    |
|                      WALL-CLOCK TIME = 5 SEGUNDOS                     |
+-----------------------------------------------------------------------+
     │                      │                      │                      │
     ▼                      ▼                      ▼                      ▼
┌─────────┐            ┌─────────┐            ┌─────────┐            ┌─────────┐
│ Core 0  │            │ Core 1  │            │ Core 2  │            │ Core 3  │
│ 5s CPU  │            │ 5s CPU  │            │ 5s CPU  │            │ 5s CPU  │
└─────────┘            └─────────┘            └─────────┘            └─────────┘
     └──────────────────────┼──────────────────────┼──────────────────────┘
                            ▼
               TOTAL CPU TIME = 20 SEGUNDOS
```
*Figura 1: Representação gráfica comparando o tempo de relógio decorrido (Wall-Clock) e a soma do tempo de CPU de todos os núcleos.*

---

### 2.1. Wall-Clock Time (Tempo de Relógio da Parede)
* **Definição:** É o tempo real decorrido no mundo físico entre o instante em que o programa começa a rodar (botão de início) e o instante em que ele termina (saída). É o tempo que um usuário percebe olhando para o relógio na parede.
* **O que inclui:** Computação útil da CPU, tempo de espera por I/O (leitura de disco), latência de rede (troca de mensagens MPI), sobrecarga de sincronização (*barriers*, *locks*) e tempo em que o processo ficou suspenso pelo Sistema Operacional.
* **Aplicações:** É a **métrica definitiva para calcular o Speedup**. O usuário final quer saber quanto tempo esperou pela resposta.

---

### 2.2. CPU Time (Tempo de Processador Acumulado)
* **Definição:** É a soma do tempo em que todos os núcleos de CPU estiveram executando instruções ativamente em nome do processo.
* **Cálculo:** Se uma aplicação utiliza 8 *threads* em 8 núcleos a 100% de capacidade durante 5 segundos de *Wall-Clock*, o **CPU Time** total será:
  $$\text{CPU Time} = 8 \text{ threads} \times 5 \text{ segundos} = 40 \text{ segundos}$$
* **Armadilha Comum:** A função tradicional `clock()` da biblioteca padrão C (`<time.h>`) retorna o **CPU Time acumulado de todas as threads**. Em um código multithreaded, chamar `clock()` no início e no fim dará um tempo 8 vezes maior do que o tempo real!

---

### 2.3. Como Medir o Tempo Corretamente em C/OpenMP/MPI?

| Ambiente / API | Função Recomendada | Resolução | Observação |
| :--- | :--- | :--- | :--- |
| **POSIX C (Linux)** | `clock_gettime(CLOCK_MONOTONIC, &ts)` | Nanossegundos | Imune a alterações de relógio do sistema (NTP). |
| **OpenMP** | `omp_get_wtime()` | Micro/Nanossegundos | Retorna o Wall-Clock Time em segundos (ponto flutuante). |
| **MPI** | `MPI_Wtime()` | Micro/Nanossegundos | Retorna o Wall-Clock Time no nó local em segundos. |
| **C Padrão (Evitar!)**| `clock()` | Milissegundos | ❌ Mede CPU Time somado; inutilizável em código paralelo! |

---

## 3. Aceleração (*Speedup* - $S_p$)

O **Speedup** ($S_p$) quantifica quantas vezes o programa paralelo é **mais rápido** em comparação com a execução em um único processador/núcleo.

### 3.1. Definição Matemática

$$S_p = \frac{T_1}{T_p}$$

Onde:
* $p$: Número de processadores (ou núcleos/threads).
* $T_1$: Tempo de execução do programa rodando em **1 único processador**.
* $T_p$: Tempo de execução do programa rodando em **$p$ processadores**.

---

### 3.2. Speedup Relativo vs. Speedup Absoluto (A Regra de Ouro!)

Existem duas formas de definir $T_1$, e essa distinção é uma fonte constante de erros e fraudes científicas:

1. **Speedup Relativo:** $T_1$ é o tempo do **mesmo código paralelo** configurado para rodar com apenas 1 thread (`OMP_NUM_THREADS=1`).
   * *Problema:* O código paralelo contém sobretaxas adicionais (*overhead* de gerenciamento de threads, rotinas de divisão de laço) que não existiriam no código sequencial mais puro.
2. **Speedup Absoluto (Real):** $T_1$ é o tempo do **melhor algoritmo sequencial possível** rodando em 1 núcleo.
   * *Exemplo:* Se o seu código paralelo usa *Bitonic Sort* em paralelo (que realiza $O(n \log^2 n)$ operações), $T_1$ deve ser o tempo do *Quicksort* ou *Radix Sort* sequencial ($O(n \log n)$).

> **💡 Regra Científica:** Sempre que possível, utilize o **Speedup Absoluto** para demonstrar o ganho real do paralelismo frente ao melhor código sequencial existente.

---

### 3.3. Categorias de Speedup

```
     Speedup (Sp)
       ▲
    4 ─┼─────────────────────────────── / (Speedup Ideal / Linear: Sp = p)
      │                             /
    3 ─┼───────────────────────────/─── (Speedup Sublinear - Típico)
      │                         . /
    2 ─┼───────────────────── .  /
      │                  .      /
    1 ─┼────────────── .       /
      │  . . . . . .          /
    0 └─┴───────────┴─────────┴─────────┴─────► Número de Cores (p)
        1           2         3         4
```
*Figura 2: Comparação visual entre Speedup Linear Ideal e Speedup Sublinear Típico.*

1. **Speedup Linear (Ideal) [$S_p = p$]:**
   * Se dobramos o número de núcleos ($p=2$), o tempo cai pela metade ($S_2 = 2$). Se usamos 100 núcleos, o programa roda 100 vezes mais rápido.
   * Na prática, é o teto teórico em um mundo sem atrito.
2. **Speedup Sublinear [$S_p < p$]:**
   * É o cenário **real e padrão** na imensa maioria das aplicações.
   * À medida que $p$ cresce, custos de comunicação, sincronização, contenção de memória e trechos sequenciais impedem que o ganho acompanhe o crescimento linear.
3. **Speedup Superlinear [$S_p > p$]:**
   * Acontece quando o programa rodando em $p$ núcleos fica **mais de $p$ vezes mais rápido** do que em 1 núcleo ($S_p > p$).
   * *Parece violar a física, mas tem uma explicação bem concreta!*

---

### 3.4. O "Milagre" do Speedup Superlinear: Por que acontece?

O Speedup Superlinear quase sempre se deve a **efeitos da arquitetura de memória (Cache Effects)**:

```
    Execução Sequencial (p = 1):
    ┌────────────────────────────────────────────────────────┐
    │ Dados Totais do Problema: 16 MB                        │
    │ Cache L3 do Core 0: apenas 4 MB                        │
    │ RESULTADO: Estouro da Cache! Milhares de Cache Misses  │
    │            buscando dados na lentíssima RAM.           │
    └────────────────────────────────────────────────────────┘

    Execução Paralela em 4 Nós/NÚCLEOS (p = 4):
    ┌──────────────┐ ┌──────────────┐ ┌──────────────┐ ┌──────────────┐
    │ Nó 0 (4 MB)  │ │ Nó 1 (4 MB)  │ │ Nó 2 (4 MB)  │ │ Nó 3 (4 MB)  │
    │ Cache L3: 4MB│ │ Cache L3: 4MB│ │ Cache L3: 4MB│ │ Cache L3: 4MB│
    └──────────────┘ └──────────────┘ └──────────────┘ └──────────────┘
    RESULTADO: O problema foi fatiado em 4 pedaços de 4 MB.
               AGORA CADA PEDAÇO CABE INTEIRO NA CACHE L3 DE CADA NÓ!
               Zero acessos à RAM! O código voa!
```
*Figura 3: O efeito de agregação de capacidade de Cache L3 gerando Speedup Superlinear.*

* **Causa 1 (Efeito de Cache):** Em 1 núcleo, os dados não cabiam na Cache L3 e causavam acessos constantes à memória RAM. Em $p$ núcleos (ou $p$ nós distribuídos), a capacidade somada de memória Cache de todos os processadores passa a ser suficiente para conter **todo o conjunto de dados**. O tempo de acesso despenca exponencialmente!
* **Causa 2 (Busca Processada / Algoritmos Não-Determinísticos):** Em algoritmos de busca (como backtracking em inteligência artificial ou árvores de decisão), uma thread paralela pode encontrar a solução certa nos primeiros milissegundos por ter tomado um caminho diferente no espaço de busca, cortando 99% do trabalho que a busca sequencial faria.

---

## 4. Eficiência ($E_p$)

O Speedup nos diz quanto o programa acelerou, mas a **Eficiência ($E_p$)** mede o **grau de aproveitamento do hardware**.

### 4.1. Definição Matemática

$$E_p = \frac{S_p}{p} = \frac{T_1}{p \cdot T_p}$$

A Eficiência é expressa como uma porcentagem (ou valor entre $0$ e $1$ / $0\%$ e $100\%$).

### 4.2. Interpretação Prática

* **$E_p = 1.0\ (100\%)$:** Aproveitamento perfeito. Todos os $p$ núcleos estão realizando trabalho útil durante $100\%$ do tempo.
* **$E_p = 0.5\ (50\%)$:** Metade do poder de processamento pago/alocado está sendo jogado fora em sobrecargas (*overhead*), espera de sincronização ou ociosidade.

#### Exemplo Numérico:
Imagine uma simulação científica rodando em um servidor de 32 núcleos:
* Tempo em 1 núcleo ($T_1$): 320 segundos.
* Tempo em 32 núcleos ($T_{32}$): 16 segundos.
* **Speedup ($S_{32}$):** $S_{32} = \frac{320}{16} = 20\times$ (O programa ficou 20 vezes mais rápido).
* **Eficiência ($E_{32}$):** $E_{32} = \frac{20}{32} = 0.625 = 62.5\%$

> **Conclusão:** Conseguimos um ganho de $20\times$, mas $37.5\%$ da capacidade computacional dos 32 núcleos foi "desperdiçada" gerando e sincronizando as threads.

---

## 5. Escalabilidade: Forte vs. Fraca (*Strong vs. Weak Scaling*)

A **Escalabilidade** é a capacidade de um sistema paralelo manter seu desempenho e eficiência à medida que aumentamos os recursos computacionais (núcleos, nós, processadores).

Existem duas formas fundamentais de avaliar a escalabilidade de uma aplicação:

```
========================================================================
1. ESCALABILIDADE FORTE (Strong Scaling)
   - Tamanho do Problema: FIXO (ex: Simular uma grade de 1.000 x 1.000)
   - Número de Processadores (p): AUMENTA (1 -> 2 -> 4 -> 8 -> 16 -> 32)
   - Objetivo: Resolver o MESMO problema o MAIS RÁPIDO POSSÍVEL.
========================================================================

========================================================================
2. ESCALABILIDADE FRACA (Weak Scaling)
   - Tamanho do Problema: AUMENTA proporcionalmente a p (Trabalho/Proc FIXO)
     (ex: 1 Core = 1.000x1.000 | 4 Cores = 2.000x2.000 | 16 Cores = 4.000x4.000)
   - Número de Processadores (p): AUMENTA (1 -> 4 -> 16)
   - Objetivo: Resolver um problema MUITO MAIOR no MESMO TEMPO.
========================================================================
```

---

### 5.1. Escalabilidade Forte (*Strong Scaling*)
* **Premissa:** O trabalho total a ser realizado é constante.
* **Comportamento Esperado:** À medida que adicionamos mais processadores, o tempo de execução $T_p$ deve diminuir.
* **O Limite Físico (Ponto de Saturação):** Chega um momento em que a fatia de trabalho enviada para cada núcleo fica tão pequena que o tempo para enviar e sincronizar a tarefa é **maior do que o tempo para calcular**. A partir desse ponto, adicionar mais núcleos **aumenta** o tempo de execução!

```
Tempo de Execução (Tp)
  ▲
  │    │   \ 
  │    \  
  │     \___
  │         \_______  <-- Ponto Ótimo
  │                 \───────────────/  <-- Região de Saturação (Overhead domina)
  └───────────────────────────────────► Número de Processadores (p)
```
*Figura 4: Curva típica de Strong Scaling mostrando a saturação e perda de desempenho por excesso de decomposição.*

---

### 5.2. Escalabilidade Fraca (*Weak Scaling*)
* **Premissa:** A quantidade de trabalho atribuída a **cada processador individual** permanece estritamente constante.
* **Comportamento Esperado:** Se dobrarmos o problema e dobrarmos os processadores, o tempo de execução ideal $T_p$ deve permanecer **perfeitamente constante** ($T_1 = T_2 = T_4 = \dots = T_p$).
* **Importância em Supercomputação:** Na prática, quando cientistas ganham acesso a um supercomputador 10 vezes maior, eles não querem rodar a mesma simulação antiga em 1 minuto em vez de 10 minutos. Eles querem simular o clima de todo o planeta com resolução 10 vezes mais detalhada no mesmo período de 24 horas!

---

## 6. Exemplo Prático em C/OpenMP: Medição Rigorosa de Métricas

Abaixo está um código em C completo que calcula a aproximação da constante $\pi$ via integração numérica. O código mede o tempo de execução com `omp_get_wtime()` e calcula dinamicamente o **Speedup** e a **Eficiência** para diferentes contagens de threads.

```c
#include <stdio.h>
#include <stdlib.h>
#include <omp.h>

// Quantidade de intervalos para a integração numérica (trabalho fixo)
static long num_steps = 500000000;

int main(int argc, char* argv[]) {
    int max_threads = omp_get_max_threads();
    double step = 1.0 / (double) num_steps;
    double t_start, t_end;
    double t_1 = 0.0; // Guardará o tempo de execução sequencial/1 thread

    printf("===============================================================
");
    printf("   AVALIAÇÃO DE DESEMPENHO: CÁLCULO DE PI VIA OPENMP           
");
    printf("   Passos de Integração: %ld                                   
", num_steps);
    printf("===============================================================

");

    printf("%-10s %-15s %-15s %-15s
", "Threads(p)", "Tempo (s)", "Speedup (Sp)", "Eficiência (Ep)");
    printf("---------------------------------------------------------------
");

    // Testa a execução variando de 1 até o número máximo de threads do sistema
    for (int p = 1; p <= max_threads; p *= 2) {
        double pi = 0.0;
        double sum = 0.0;

        t_start = omp_get_wtime();

        #pragma omp parallel num_threads(p) private(sum)
        {
            double x;
            #pragma omp for reduction(+:pi) schedule(static)
            for (long i = 0; i < num_steps; i++) {
                x = (i + 0.5) * step;
                pi += 4.0 / (1.0 + x * x);
            }
        }

        pi = step * pi;
        t_end = omp_get_wtime();

        double elapsed = t_end - t_start;

        if (p == 1) {
            t_1 = elapsed; // Guarda o tempo base de 1 thread
        }

        double speedup = t_1 / elapsed;
        double efficiency = speedup / p;

        printf("%-10d %-15.4f %-15.2f %-15.2f%%
", 
               p, elapsed, speedup, efficiency * 100.0);
    }

    printf("===============================================================
");
    return 0;
}
```

### Exemplo de Saída do Programa (Servidor Octa-Core):
```text
===============================================================
   AVALIAÇÃO DE DESEMPENHO: CÁLCULO DE PI VIA OPENMP           
   Passos de Integração: 500000000                                   
===============================================================

Threads(p)  Tempo (s)       Speedup (Sp)    Eficiência (Ep)
---------------------------------------------------------------
1          1.2450          1.00            100.00%        
2          0.6280          1.98            99.12%         
4          0.3190          3.90            97.57%         
8          0.1680          7.41            92.63%         
===============================================================
```

---

## 7. Resumo e Tabela Consolidada

| Métrica | Fórmula / Conceito | Significado Prático |
| :--- | :--- | :--- |
| **Wall-Clock Time ($T_p$)** | Tempo real cronometrado | Tempo físico de espera do usuário. Base para o cálculo de Speedup. |
| **CPU Time** | $\sum T_{\text{core}}$ | Tempo total de processamento somado de todos os núcleos. |
| **Speedup ($S_p$)** | $S_p = \frac{T_1}{T_p}$ | Quantas vezes o sistema ficou mais rápido com $p$ processadores. |
| **Eficiência ($E_p$)** | $E_p = \frac{S_p}{p}$ | Porcentagem da capacidade do hardware que é aproveitada em trabalho útil. |
| **Strong Scaling** | Problema Fixo, $p$ Aumenta | Busca reduzir o tempo do mesmo problema até atingir o limite de sobrecarga. |
| **Weak Scaling** | Trabalho/Proc Fixo, $p$ Aumenta | Busca resolver problemas proporcionalmente maiores mantendo o tempo constante. |

---

## 📝 Exercícios de Fixação e Reflexão

1. **(Cálculo de Métricas)** Uma aplicação de dinâmica molecular leva 120 segundos para rodar em 1 núcleo. Quando executada em um nó com 16 núcleos, o tempo cai para 10 segundos.
   * a) Qual é o Speedup ($S_{16}$)?
   * b) Qual é a Eficiência ($E_{16}$)?
   * c) Houve aceleração linear perfeita? Justifique.

2. **(Análise Conceitual)** O que é o *Speedup Superlinear* e por que ele não viola as leis da física? Dê um exemplo baseado na arquitetura de memória cache.

3. **(Escalabilidade)** Um laboratório de meteorologia precisa escolher entre duas abordagens para um novo supercomputador:
   * Abordagem A: Reduzir a previsão do tempo de amanhã de 2 horas para 15 minutos.
   * Abordagem B: Manter o tempo em 2 horas, mas aumentar a precisão espacial do mapa de 10 km para 1 km de resolução.
   * *Pergunta:* Qual das abordagens reflete um teste de **Strong Scaling** e qual reflete um teste de **Weak Scaling**?

---

### Gabarito Resumido dos Exercícios

1. **Respostas:**
   * a) $S_{16} = \frac{120}{10} = 12\times$.
   * b) $E_{16} = \frac{12}{16} = 0.75 = 75\%$.
   * c) Não. O Speedup linear perfeito seria $16\times$. Os $25\%$ perdidos são decorrentes do overhead de comunicação, sincronização ou partes sequenciais do código.
2. **Resposta:** O Speedup Superlinear ocorre quando $S_p > p$. Ele não viola a física porque se deve a ganhos estruturais de hardware: ao fatiar o problema em $p$ partes menores, cada parte passa a couber inteira na memória Cache L2/L3 de cada núcleo, eliminando acessos lentos à RAM que ocorriam na execução em 1 único núcleo.
3. **Resposta:** A **Abordagem A** reflete **Strong Scaling** (tamanho do problema fixo, busca reduzir o tempo). A **Abordagem B** reflete **Weak Scaling** (aumenta o problema proporcionalmente ao aumento de hardware para manter o tempo fixo).
