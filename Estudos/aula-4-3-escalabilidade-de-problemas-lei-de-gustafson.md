# Aula 4.3: Escalabilidade de Problemas — A Lei de Gustafson e o Speedup Dimensionado

---

## 1. Introdução: A Provocação de John Gustafson

Na **Aula 4.2**, estudamos a **Lei de Amdahl** e vimos uma previsão desanimadora: se um programa possui **10% de código sequencial** ($f = 0{,}10$), o *speedup* máximo teórico jamais ultrapassará **10x**, mesmo que você utilize um supercomputador com **1.000.000 de núcleos**.

Durante as décadas de 1970 e 1980, esse resultado fez muitos cientistas da computação acreditarem que a computação paralela de altíssimo desempenho (*Massively Parallel Processing*) seria um fracasso comercial e prático. Afinal, para que investir milhões em milhares de processadores se o ganho de velocidade encontrava um "teto" tão baixo?

Em **1988**, no *Sandia National Laboratories*, o pesquisador **John L. Gustafson** publicou um artigo revolucionário intitulado *"Reevaluating Amdahl's Law"* (Reavaliando a Lei de Amdahl). Gustafson demonstrou na prática, utilizando um supercomputador de 1.024 processadores (o NCUBE 10), que era possível obter acelerações superiores a **1.000x** em problemas reais de física e engenharia.

> **A grande sacada de Gustafson:** A Lei de Amdahl comete uma premissa irreal sobre o comportamento humano e científico. Ela assume que o **tamanho do problema é fixo** e que o usuário apenas deseja resolver o *mesmo problema pequeno mais rápido*. Na prática, quando cientistas ganham computadores 1.000 vezes mais potentes, eles não rodam a mesma simulação de 10 segundos em 10 milissegundos; eles usam o poder extra para rodar uma simulação **1.000 vezes mais detalhada e precisa** no mesmo intervalo de tempo de 1 hora!

---

## 2. A Analogia Central: A Viagem vs. A Capacidade de Carga

Para entender a diferença conceitual entre Amdahl e Gustafson, imagine a tarefa de transportar cargas.

### A Abordagem de Amdahl (Tamanho Fixo do Problema)
* **Cenário:** Você precisa transportar exatamente **10 caixas** de uma cidade A para uma cidade B.
* Se você contratar 10 caminhões, cada um leva 1 caixa.
* No entanto, o tempo para carregar a primeira caixa no galpão e assinar a nota fiscal é fixo e sequencial. Não importa quantos caminhões você adicione, a assinatura do papel leva o mesmo tempo.
* Adicionar mais 100 caminhões não ajudará em nada, pois o trabalho total é pequeno demais para compensar a logística.

### A Abordagem de Gustafson (Tempo Fixo de Execução / Carga Expansível)
* **Cenário:** Você tem uma janela rígida de **1 hora** para transportar a maior quantidade possível de suprimentos para uma zona de resgate.
* Se você tiver apenas **1 caminhão**, em 1 hora ele consegue carregar e entregar **10 caixas**.
* Se você contratar **1.000 caminhões**, no mesmo intervalo de **1 hora** a equipe conseguirá organizar, carregar e transportar **10.000 caixas**!
* O tempo total de trabalho permaneceu **1 hora**, mas o **volume de trabalho útil realizado explodiu**.

```
   [ Visão de Amdahl: Strong Scaling ]
   Fixa o Tamanho do Problema (N) ---> O tempo de execução cai até esbarrar na fração sequencial.
   
   [ Visão de Gustafson: Weak Scaling ]
   Fixa o Tempo de Execução (T_p) ---> O tamanho do problema (N) cresce proporcionalmente aos processadores!
```

---

## 3. Dedução Matemática da Lei de Gustafson

Vamos deduzir a fórmula matemática do **Scaled Speedup** (Aceleração Dimensionada) formulada por Gustafson.

### 3.1. Definindo as Variáveis no Tempo Paralelo
Em vez de começar fixando o trabalho total de 1 processador, Gustafson começou fixando o tempo de execução no **sistema paralelo com $P$ processadores**:

Definimos o tempo paralelo total como normalizado para **$T_p = 1$**:
$$T_p = s + p = 1$$

Onde:
* $s$: É o tempo gasto na parte **sequencial** da aplicação no sistema paralelo.
* $p$: É o tempo gasto na parte **paralela** da aplicação no sistema paralelo.
* Como $s + p = 1$, temos que $p = 1 - s$.

### 3.2. Calculando o Tempo que 1 Único Processador Levaria
Se fôssemos executar esse **novo problema expandido** em apenas **1 único processador**:
1. A parte sequencial levaria o mesmo tempo: $s$.
2. A parte paralela, que foi dividida entre $P$ processadores no sistema paralelo, agora precisará ser executada sequencialmente, uma parte de cada vez, por aquele único processador. Portanto, levará $p 	imes P$.

O tempo de execução sequencial $T_1$ para a carga de trabalho expandida é:
$$T_1 = s + p \cdot P$$

### 3.3. A Fórmula do Scaled Speedup ($S_{	ext{scaled}}$)
Substituindo $T_1$ e $T_p$ na fórmula fundamental do *Speedup* ($S = rac{T_1}{T_p}$):

$$S_{	ext{scaled}} = rac{T_1}{T_p} = rac{s + p \cdot P}{1} = s + p \cdot P$$

Como sabemos que $p = 1 - s$, podemos substituir $p$:

$$S_{	ext{scaled}} = s + (1 - s) \cdot P$$

Expandindo a equação:
$$S_{	ext{scaled}} = s + P - s \cdot P$$
$$S_{	ext{scaled}} = P - s(P - 1)$$

---

### 3.4. O Impacto da Fórmula na Prática
Examine a equação resultante:
$$S_{	ext{scaled}} = P - s(P - 1)$$

Observe que $S_{	ext{scaled}}$ é uma **função linear** em relação ao número de processadores $P$!

* Se a fração sequencial $s$ for de **10%** ($s = 0{,}10$) e usarmos $P = 1.024$ processadores:
  * **Pela Lei de Amdahl:** $S_{	ext{max}} = rac{1}{0{,}10} = 10	ext{x}$ (desastre!).
  * **Pela Lei de Gustafson:** $S_{	ext{scaled}} = 1.024 - 0{,}10 	imes (1.024 - 1) = 1.024 - 102{,}3 = 921{,}7	ext{x}$!

> **Conclusão:** À medida que aumentamos o tamanho do problema proporcionalmente ao número de processadores, a fração paralela $p$ domina o tempo total, e o impacto da parte sequencial $s$ torna-se insignificante!

---

## 4. Amdahl vs. Gustafson: A Grande Comparação

A tabela a seguir consolida as diferenças fundamentais entre os dois modelos de escalabilidade:

| Característica | **Lei de Amdahl** | **Lei de Gustafson** |
| :--- | :--- | :--- |
| **Premissa de Carga** | Tamanho do problema é **Fixo** ($N = 	ext{constante}$). | Tempo de execução é **Fixo** ($T_p = 	ext{constante}$). |
| **Tipo de Escalabilidade** | **Escalabilidade Forte** (*Strong Scaling*). | **Escalabilidade Fraca** (*Weak Scaling*). |
| **Objetivo do Usuário** | Resolver o mesmo problema no **menor tempo**. | Resolver o **maior problema possível** no tempo aceitável. |
| **Comportamento da Curva** | Achata rapidamente (*Teto limite $1/f$*). | Cresce **linearmente** com o número de núcleos $P$. |
| **Aplicação Típica** | Algoritmos de tempo real, busca rápida em banco de dados. | Simulações científicas (clima, astrofísica, IA, dinâmica molecular). |

```
   [ INDICAÇÃO DE IMAGEM / DIAGRAMA ]
   Nome da Imagem: amdahl_vs_gustafson_curves.png
   Descrição Visual: Gráfico de linha comparando Speedup (Eixo Y: 0 a 100) vs Número de Processadores P (Eixo X: 0 a 100).
   - Linha Vermelha (Amdahl): Sobe rapidamente e dobra assintoticamente no teto Y=10 para 10% sequencial.
   - Linha Azul (Gustafson): Uma reta quase diagonal perfeita subindo linearmente de P=1 a P=100.
```

---

## 5. Exemplo Prático em C/OpenMP: Demonstrando Weak Scaling

Para ver a Lei de Gustafson em ação, vamos implementar um programa que realiza uma simulação numérica pesada (cálculo de pi por integração) aplicando **Weak Scaling**.

Em **Weak Scaling**, quando dobramos o número de *threads*, **dobramos o número de pontos de integração ($N$)**, mantendo a quantidade de trabalho por *thread* constante!

```c
#include <stdio.h>
#include <stdlib.h>
#include <omp.h>

int main() {
    // Quantidade BASE de trabalho por thread (100 milhões de pontos por thread)
    long WORK_PER_THREAD = 100000000L;
    
    printf("===============================================================
");
    printf("     DEMONSTRAÇÃO DA LEI DE GUSTAFSON (WEAK SCALING)          
");
    printf("===============================================================

");
    
    printf("Threads | Tamanho do Problema (N) | Tempo (s) | Scaled Speedup
");
    printf("---------------------------------------------------------------
");

    double time_single_thread = 0.0;

    // Testando com 1, 2, 4 e 8 threads
    int thread_counts[] = {1, 2, 4, 8};
    int num_tests = sizeof(thread_counts) / sizeof(thread_counts[0]);

    for (int t = 0; t < num_tests; t++) {
        int threads = thread_counts[t];
        
        // NOVO TAMANHO DO PROBLEMA: Cresce proporcionalmente às threads!
        long N = WORK_PER_THREAD * threads;
        double step = 1.0 / (double)N;
        double sum = 0.0;

        omp_set_num_threads(threads);

        double start_time = omp_get_wtime();

        // Seção Paralela com redução
        #pragma omp parallel for reduction(+:sum)
        for (long i = 0; i < N; i++) {
            double x = (i + 0.5) * step;
            sum += 4.0 / (1.0 + x * x);
        }

        double pi = sum * step;
        double end_time = omp_get_wtime();
        double execution_time = end_time - start_time;

        if (threads == 1) {
            time_single_thread = execution_time;
        }

        // Scaled Speedup = Trabalho Realizado / Tempo Relativo
        // Como o trabalho cresceu por um fator de 'threads' e o tempo ideal é constante:
        double scaled_speedup = ((double)N / WORK_PER_THREAD) * (time_single_thread / execution_time);

        printf("%7d | %22ld | %9.4f | %14.2fx\n", 
               threads, N, execution_time, scaled_speedup);
    }

    printf("---------------------------------------------------------------
");
    printf("Nota: Observe que o tempo de execução permanece quase CONSTANTE
");
    printf("      enquanto a quantidade total de dados processados explode!
");

    return 0;
}
```

### O que esperar do resultado acima?
* Com **1 thread** ($N = 100	ext{M}$): Tempo $pprox 0{,}25	ext{s}$ | *Scaled Speedup* = $1{,}00	ext{x}$.
* Com **2 threads** ($N = 200	ext{M}$): Tempo $pprox 0{,}26	ext{s}$ | *Scaled Speedup* = $1{,}92	ext{x}$.
* Com **4 threads** ($N = 400	ext{M}$): Tempo $pprox 0{,}27	ext{s}$ | *Scaled Speedup* = $3{,}70	ext{x}$.
* Com **8 threads** ($N = 800	ext{M}$): Tempo $pprox 0{,}29	ext{s}$ | *Scaled Speedup* = $6{,}90	ext{x}$.

> O tempo quase não mudou, mas o programa processou **800 milhões de elementos** em vez de 100 milhões! É a Lei de Gustafson provada na prática.

---

## 6. Aplicações Práticas de Weak Scaling em HPC

Onde a visão de Gustafson é aplicada no mundo real?

1. **Previsão Meteorológica e Clima Global:**
   * Em vez de tentar calcular a previsão do tempo para os próximos 5 dias em 1 segundo (impossível devido às limitações de rede e I/O), os supercomputadores usam mais nós para reduzir o tamanho da grade da simulação de **50 km para 1 km de resolução**.
2. **Treinamento de Modelos de Inteligência Artificial (LLMs):**
   * Treinar modelos como GPT-4 ou Gemini exige clusters com dezenas de milhares de GPUs. O tamanho dos conjuntos de dados e os parâmetros das redes expandem junto com a adição de mais nós de aceleração.
3. **Simulações de Dinâmica Molecular e Astrofísica:**
   * Modelagem de milhões de partículas ou corpos celestes (Problema dos $N$-Corpos) em vez de apenas centenas.

---

## 7. Resumo do Módulo e Exercícios de Fixação

### Resumo das Ideias-Chave:
* **Lei de Amdahl:** Modela a **Strong Scaling** (tamanho do problema fixo). Prevê limitações severas impostas por $f$.
* **Lei de Gustafson:** Modela a **Weak Scaling** (tempo fixo, problema expansível). Demonstra que o *speedup* escala linearmente com $P$ se aumentarmos os dados.
* **Fórmula do Scaled Speedup:** $S_{	ext{scaled}} = P - s(P - 1)$.

---

### Exercícios de Fixação

#### Questão 1
Se uma aplicação paralela possui uma fração sequencial $s = 0{,}05$ (5%) quando executada em um supercomputador com $P = 512$ processadores:
a) Qual é o *Speedup* máximo previsto pela **Lei de Amdahl**?
b) Qual é o *Scaled Speedup* previsto pela **Lei de Gustafson**?

*Gabarito Comentado:*
a) Amdahl: $S_{	ext{max}} = rac{1}{f} = rac{1}{0{,}05} = 20	ext{x}$.
b) Gustafson: $S_{	ext{scaled}} = P - s(P - 1) = 512 - 0{,}05(512 - 1) = 512 - 0{,}05(511) = 512 - 25{,}55 = 486{,}45	ext{x}$.

---

#### Questão 2
Qual é a diferença conceitual entre *Strong Scaling* e *Weak Scaling*?
* **A)** Strong Scaling varia o número de processadores mantendo o tempo fixo; Weak Scaling varia o tempo mantendo os dados fixos.
* **B)** Strong Scaling mantém o tamanho total do problema constante enquanto adiciona processadores; Weak Scaling aumenta o tamanho do problema proporcionalmente ao número de processadores.
* **C)** Strong Scaling só se aplica a CPUs; Weak Scaling só se aplica a GPUs.
* **D)** Nenhuma, ambas as métricas medem exatamente o mesmo fenômeno sob as mesmas condições.

*Gabarito:* **B**.
