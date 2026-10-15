# Aula 4.2: Limites Teóricos — A Lei de Amdahl e a Fração Sequencial Crítica

---

## 🎯 Objetivos de Aprendizagem
Ao final desta aula, você será capaz de:
1. Entender a **Lei de Amdahl** e a origem da fórmula teórica do *speedup*.
2. Identificar a **Fração Sequencial Crítica ($f$)** de um algoritmo e calcular o limite máximo de aceleração ($S_{\text{max}} = 1/f$).
3. Compreender a **curva de retornos decrescentes** ao adicionar mais processadores em um sistema.
4. Aplicar a Lei de Amdahl em códigos reais em C/OpenMP para diagnosticar gargalos sequenciais.
5. Reconhecer os mitos do "paralelismo infinito" e saber quando parar de adicionar *hardware*.

---

## 💡 Analogia Central: O Bolo no Forno e a Reforma da Casa

Imagine que você está organizando uma grande festa e precisa preparar uma refeição completa:
* **Fase Paralelizável (90% do tempo total):** Picar vegetais, temperar carnes, lavar louça, arrumar as mesas. Se você contratar **10 cozinheiros**, essa etapa pode ser feita 10 vezes mais rápido!
* **Fase Estritamente Sequencial (10% do tempo total):** Assar o bolo principal no forno. Não importa quantos cozinheiros estejam na cozinha, o bolo leva obrigatoriamente **1 hora** para assar devido às leis da física e da química.

```
[--- Picar/Temperar/Arrumar (Paralelo: 90%) ---][--- Assar o Bolo (Sequencial: 10%) ---]
```

Mesmo que você contrate **1.000.000 de cozinheiros**, o tempo de preparação nunca será menor do que **1 hora** (o tempo do forno). 

Na computação paralela acontece exatamente a mesma coisa: **A fração estritamente sequencial do seu código impõe um teto inegociável ao ganho de desempenho, não importa quantos núcleos de CPU você utilize!**

---

## 📐 A Matemática da Lei de Amdahl

Formulada por Gene Amdahl em 1967, a **Lei de Amdahl** modela a aceleração máxima (*speedup*) que um programa pode obter ao utilizar $p$ processadores.

### 1. Definição dos Parâmetros
* $T_1$ (ou $T_s$): Tempo total de execução do programa em um único processador (sequencial).
* $f$: **Fração sequencial** do código (porcentagem do tempo que *não* pode ser paralelizada, expressa entre 0 e 1).
* $(1 - f)$: **Fração paralelizável** do código.
* $p$: Número de processadores/núcleos alocados.

### 2. O Tempo de Execução Paralelo ($T_p$)
O tempo total no sistema paralelo é a soma da parte que roda em um único núcleo mais a parte dividida entre os $p$ núcleos:

$$T_p = T_1 \cdot f + \frac{T_1 \cdot (1 - f)}{p}$$

### 3. A Formulação do Speedup ($S_p$)
Sabendo que $S_p = \frac{T_1}{T_p}$, substituímos a equação acima:

$$S_p = \frac{T_1}{T_1 \cdot f + \frac{T_1 \cdot (1 - f)}{p}} = \frac{1}{f + \frac{1 - f}{p}}$$

---

### 4. O Limite Teórico Máximo ($p \to \infty$)
Se tivermos um número **infinito** de processadores ($p \to \infty$), a parcela paralelizável $\frac{1-f}{p}$ tende a zero:

$$S_{\text{max}} = \lim_{p \to \infty} \frac{1}{f + \frac{1 - f}{p}} = \frac{1}{f}$$

> 🔴 **Regra de Ouro de Amdahl:**
> Se **$f = 5\%$** (0.05) do seu código for sequencial, o *speedup* máximo teórico é **$1 / 0.05 = 20\times$**, mesmo que você use um supercomputador com **1.000.000 de cores**!

---

## 📊 Análise Numérica e a Curva de Retornos Decrescentes

Vamos observar o impacto do número de processadores ($p$) para diferentes frações sequenciais ($f$):

| Processadores ($p$) | $f = 50\%$ (S_p) | $f = 25\%$ (S_p) | $f = 10\%$ (S_p) | $f = 1\%$ (S_p) |
| :---: | :---: | :---: | :---: | :---: |
| **1** | 1.00x | 1.00x | 1.00x | 1.00x |
| **2** | 1.33x | 1.60x | 1.82x | 1.98x |
| **4** | 1.60x | 2.29x | 3.08x | 3.88x |
| **8** | 1.78x | 2.91x | 4.71x | 7.48x |
| **16** | 1.88x | 3.40x | 6.40x | 13.91x |
| **64** | 1.97x | 3.82x | 8.77x | 39.26x |
| **256** | 1.99x | 3.95x | 9.66x | 72.11x |
| **$\\infty$ (Infinito)** | **2.00x** | **4.00x** | **10.00x** | **100.00x** |

![Gráfico da Lei de Amdahl mostrando a saturação do speedup à medida que o número de processadores aumenta](/workspace/scratch/aula4_2/amdahl_law_graph.png)
*(Descrição da Imagem: Um gráfico cartesiano onde o eixo X representa o número de núcleos de CPU (1 a 128) e o eixo Y representa o Speedup. Diversas curvas para f = 50%, 25%, 10%, 5% e 1% mostram que todas as curvas se achatam horizontalmente em 1/f, evidenciando a saturação).*

```
Speedup (S_p)
  ^
10|                                  ----------- f = 10% (Teto em 10x)
  |                             /----
 5|                   /---------             f = 20% (Teto em 5x)
  |             /-----
 2|       /-----                             f = 50% (Teto em 2x)
  |  /----
 1+------------------------------------------------------------>
  1    2    4    8    16   32   64   128  256   ...      Processadores (p)
```

### O que este gráfico nos ensina?
1. **Retornos Decrescentes:** Adicionar os primeiros 4 núcleos traz um ganho expressivo. Passar de 64 para 256 núcleos em $f=10\%$ aumenta o speedup apenas de $8.77\times$ para $9.66\times$ (um desperdício enorme de energia e recursos).
2. **A "Ditadura" da Fração Sequencial:** Para obter acelerações de centenas de vezes, a fração sequencial $f$ precisa ser virtualmente zero ($f < 0.1\%$).

---

## 💻 Exemplo Prático em C / OpenMP: Medindo a Fração Sequencial em Código Real

Vamos analisar um programa que possui uma etapa sequencial (leitura/alocação de memória) e uma etapa paralela (cálculo numérico).

```c
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <omp.h>

#define N 50000000

// Etapa Sequencial: Inicialização do vetor
void inicializa_dados(double *v, int n) {
    for (int i = 0; i < n; i++) {
        v[i] = (double)(i % 100) / 10.0;
    }
}

// Etapa Paralelizável: Processamento matemático pesado
double processa_dados_paralelo(double *v, int n, int num_threads) {
    double soma = 0.0;
    
    #pragma omp parallel for num_threads(num_threads) reduction(+:soma)
    for (int i = 0; i < n; i++) {
        // Operação pesada para simular carga computacional
        v[i] = sqrt(v[i]) * sin(v[i]) + exp(0.001 * v[i]);
        soma += v[i];
    }
    
    return soma;
}

int main() {
    double *vetor = (double *)malloc(N * sizeof(double));
    
    // 1. Medir tempo total sequencial (1 thread)
    double t_inicio_seq = omp_get_wtime();
    
    // Fase Sequencial
    inicializa_dados(vetor, N); 
    double t_fim_fase_seq = omp_get_wtime();
    
    // Fase Paralela com 1 thread para baseline
    double soma_base = processa_dados_paralelo(vetor, N, 1);
    double t_fim_total_seq = omp_get_wtime();
    
    double tempo_fase_seq = t_fim_fase_seq - t_inicio_seq;
    double tempo_total_seq = t_fim_total_seq - t_inicio_seq;
    
    // Calcular a fração sequencial real f
    double f = tempo_fase_seq / tempo_total_seq;
    double speedup_max_teorico = 1.0 / f;
    
    printf("==================================================\n");
    printf("   ANÁLISE DE AMDAHL - DIAGNÓSTICO DO PROGRAMA\n");
    printf("==================================================\n");
    printf("Tempo Fase Sequencial (fase_seq): %.4f segundos\n", tempo_fase_seq);
    printf("Tempo Total Sequencial (T_1):     %.4f segundos\n", tempo_total_seq);
    printf("Fração Sequencial Medida (f):     %.2f%% (%.4f)\n", f * 100.0, f);
    printf("Speedup MÁXIMO Teórico (1/f):     %.2fx\n", speedup_max_teorico);
    printf("==================================================\n\n");
    
    printf("Threads (p) | Tempo Medido (s) | Speedup Medido | Speedup Amdahl Teórico\n");
    printf("-------------------------------------------------------------------------\n");
    
    int test_threads[] = {1, 2, 4, 8, 16};
    int num_testes = sizeof(test_threads) / sizeof(test_threads[0]);
    
    for (int i = 0; i < num_testes; i++) {
        int p = test_threads[i];
        
        double t0 = omp_get_wtime();
        // Re-executa fase sequencial (não paralelizável) + fase paralela em p threads
        inicializa_dados(vetor, N);
        processa_dados_paralelo(vetor, N, p);
        double t1 = omp_get_wtime();
        
        double t_p = t1 - t0;
        double speedup_medido = tempo_total_seq / t_p;
        double speedup_amdahl = 1.0 / (f + ((1.0 - f) / p));
        
        printf("%11d | %16.4f | %14.2fx | %22.2fx\n", 
               p, t_p, speedup_medido, speedup_amdahl);
    }
    
    free(vetor);
    return 0;
}
```

---

## 🔍 Fontes Típicas de Fração Sequencial ($f$) na Prática

Por que existe uma parte sequencial mesmo nos programas mais bem escritos?

1. **Entrada e Saída de Dados (I/O):** Leitura de arquivos do disco (NFS, HD, SSD), parsing de arquivos CSV/JSON e gravações de logs.
2. **Alocação e Inicialização de Memória:** Execução de `malloc()`, `new`, ou zeramento de matrizes grandes.
3. **Sincronizações Finais:** Reduções globais (`MPI_Reduce`, `#pragma omp reduction`), seções críticas (`critical`, `mutex`) e pontos de encontro obrigatórios (`barrier`).
4. **Comunicação e Latência de Rede:** Envio do estado inicial para os nós de um cluster no MPI.

---

## ⚠️ Mitos e Erros Comuns de Interpretação

| Mito | Realidade |
| :--- | :--- |
| *"Se eu comprar uma máquina com o dobro de núcleos, meu programa vai rodar no dobro da velocidade."* | **FALSO.** O ganho é limitado pela fração sequencial $f$. Se $f=20\%$, passar de 8 para 16 núcleos muda o speedup apenas de $3.33	imes$ para $3.80	imes$. |
| *"A Lei de Amdahl prova que paralelismo massivo (HPC) é inútil."* | **FALSO.** Ela prova que para aproveitar muitos núcleos, precisamos focar em **reduzir $f$** (usando I/O assíncrono, algoritmos sem trava) ou aumentar o tamanho do problema (que é o foco da **Lei de Gustafson**, vista na próxima aula). |
| *"A Lei de Amdahl considera os custos de comunicação."* | **FALSO.** A Lei de Amdahl é um modelo **otimista**. Na prática, a comunicação entre threads/processos adiciona sobrecarga (*overhead*), fazendo com que o speedup real fique *abaixo* da curva de Amdahl para $p$ elevado. |

---

## 📋 Tabela Resumo: Fórmulas e Relações Fundamentais

| Conceito | Fórmula | Descrição |
| :--- | :--- | :--- |
| **Tempo Paralelo** | $T_p = T_1 \cdot \left(f + rac{1-f}{p}ight)$ | Tempo estimado de execução com $p$ processadores. |
| **Speedup de Amdahl** | $S_p = rac{1}{f + rac{1-f}{p}}$ | Aceleração teórica esperada. |
| **Speedup Máximo Assintótico** | $S_{	ext{max}} = rac{1}{f}$ | Limite inatingível quando $p 	o \infty$. |
| **Fração Sequencial Crítica** | $f = rac{T_{	ext{seq}}}{T_1}$ | Razão entre o tempo não-paralelizável e o tempo total sequencial. |

---

## ✏️ Exercícios de Fixação com Gabarito Comentado

### Questão 1
Um algoritmo de processamento de imagens gasta 15% do seu tempo lendo o arquivo de imagem do disco e 85% aplicando filtros nos pixels.
1. Qual é o *speedup* máximo teórico deste programa se utilizarmos um processador com **8 núcleos**?
2. Qual é o *speedup* máximo absoluto, mesmo se tivéssemos infinitos núcleos?

<details>
<summary><b> Ver Resposta e Resolução Comentada</b></summary>

**Dados:** $f = 0.15$, $1 - f = 0.85$

1. **Para $p = 8$ núcleos:**
   $$S_8 = rac{1}{f + rac{1-f}{p}} = rac{1}{0.15 + rac{0.85}{8}} = rac{1}{0.15 + 0.10625} = rac{1}{0.25625} pprox 3.90	imes$$
   *O ganho com 8 núcleos será de aproximadamente 3.90 vezes.*

2. **Para $p 	o \infty$ (Infinitos núcleos):**
   $$S_{	ext{max}} = rac{1}{f} = rac{1}{0.15} pprox 6.67	imes$$
   *Mesmo com um supercomputador, o programa nunca rodará mais de 6.67 vezes mais rápido.*
</details>

---

### Questão 2
Uma equipe de software deseja alcançar um *speedup* de pelo menos **20 vezes** em uma aplicação financeira paralela. Qual é a porcentagem **máxima** de código sequencial ($f$) que a aplicação pode conter para que este objetivo seja teoricamente possível?

<details>
<summary><b> Ver Resposta e Resolução Comentada</b></summary>

**Dados:** $S_{	ext{max}} = 20$

Utilizando a fórmula do limite assintótico:
$$S_{	ext{max}} = rac{1}{f} \implies 20 = rac{1}{f} \implies f = rac{1}{20} = 0.05 	ext{ (ou } 5\%	ext{)}$$

*A fração sequencial do código não pode ultrapassar **5%**. Se mais de 5% do tempo for gasto em etapas sequenciais, atingir um speedup de $20	imes$ torna-se matematicamente impossível.*
</details>

---

### Questão 3
Explique por que um programa real pode apresentar um *speedup* menor do que o previsto pela fórmula de Amdahl ao utilizar 64 núcleos.

<details>
<summary><b> Ver Resposta e Resolução Comentada</b></summary>

A Lei de Amdahl é um modelo **teórico idealizado** que pressupõe que a fração paralela $(1-f)$ pode ser dividida perfeitamente entre os $p$ núcleos **sem custo adicional**. 

Na prática, com 64 núcleos surgem os seguintes custos de sobrecarga (*overheads*):
1. **Contenção e Custo de Comunicação:** Redes ou barramentos de memória ficam saturados transitando mensagens/dados.
2. **Sincronização:** Tempo gasto em esperas de barreiras (`barrier`), mutexes ou *False Sharing*.
3. **Escalonamento:** Custo do Sistema Operacional criando e gerenciando 64 threads.
   
Por isso, a curva real costuma ficar **abaixo** da curva teórica de Amdahl.
</details>

---

Com esta aula, consolidamos o entendimento de que otimizar a fase sequencial de um programa é tão importante quanto paralelizar a fase computacional! Na próxima aula (**Aula 4.3: Escalabilidade de Problemas — A Lei de Gustafson**), veremos como quebrar essa limitação ajustando o tamanho do problema! 🚀
