# Aula 2.5: Metodologia de Projeto Paralelo — O Modelo de Foster (PCAM)

## Introdução: Por que Precisamos de uma Metodologia de Projeto?

Escrever um programa concorrente ou paralelo eficiente vai muito além de simplesmente inserir diretivas como `#pragma omp parallel` em laços de repetição ou criar múltiplos *threads* de forma desordenada. Quando abordamos o desenvolvimento paralelo de forma empírica ou "no improviso", é muito comum cairmos em armadilhas clássicas:

* **Overhead de Comunicação:** *Threads* gastando mais tempo conversando e sincronizando entre si do que realizando cálculos úteis.
* **Desbalanceamento de Carga (*Load Imbalance*):** Metade dos núcleos de processamento finaliza suas tarefas em segundos e fica ociosa, enquanto um único núcleo fica sobrecarregado trabalhando por minutos.
* **Gargalos de Escalabilidade:** Códigos que rodam bem com 4 *threads*, mas estaguam ou ficam mais lentos quando executados em 64 ou 128 núcleos.

Para evitar o retrabalho e garantir que o software seja eficiente, escalável e portátil entre diferentes arquiteturas (de *multicores* de memória compartilhada a *clusters* distribuídos), utilizamos o **Modelo de Foster** (proposto pelo cientista da computação Ian Foster em 1995).

O Modelo de Foster divide o processo de design paralelo em **4 etapas sistemáticas**, conhecidas pela sigla **PCAM**:

1. **P**articionamento (*Partitioning*)
2. **C**omunicação (*Communication*)
3. **A**glomeração (*Agglomeration*)
4. **M**apeamento (*Mapping*)

---

## A Analogia Central: O Canteiro de Obras de um Grande Edifício

Para fixar a lógica do modelo PCAM, imagine que você é o engenheiro responsável por construir um arranha-céu de 50 andares:

```
+-------------------------------------------------------------------------+
|                  ANALOGIA DO CANTEIRO DE OBRAS (PCAM)                   |
+-------------------------------------------------------------------------+
|  1. PARTICEIONAMENTO : Projetar o prédio pensando em cada tijolo       |
|                       individualmente (máximo de tarefas finas).        |
|                                                                         |
|  2. COMUNICAÇÃO      : Mapear quais pedreiros precisam passar tijolos,  |
|                       cimento ou ferramentas uns para os outros.        |
|                                                                         |
|  3. AGLOMERAÇÃO      : Agrupar tijolos em paredes/andares inteiros para |
|                       fazer sentido para um operário trabalhar.         |
|                                                                         |
|  4. MAPEAMENTO       : Distribuir os andares específicos para equipes   |
|                       e guindastes reais disponíveis no canteiro.       |
+-------------------------------------------------------------------------+
```

---

## As 4 Etapas do Modelo PCAM de Foster

### 1. Particionamento (*Partitioning*)

O objetivo do particionamento é decompor o problema (dados ou rotinas de código) no **maior número possível de pequenas tarefas independentes** (chamadas de *fine-grained tasks* ou tarefas de granularidade fina).

> ⚠️ **Regra de Ouro do Particionamento:** Nesta primeira etapa, **ignore totalmente o hardware real!** Não pense se o seu computador tem 4, 8 ou 64 núcleos. Pense apenas no problema e em como expor o paralelismo máximo inerente aos dados e à lógica.

Existem duas abordagens fundamentais para o particionamento:

#### A) Decomposição de Dados (Domain Decomposition / Data Parallelism)
Dividem-se os dados manipulados pelo algoritmo em pequenas fatias.
* *Exemplo:* Em um processamento de imagem de $4000 	imes 4000$ pixels, cada tarefa fina é responsável por calcular o valor de **1 único pixel**.

#### B) Decomposição de Tarefas (Functional Decomposition / Task Parallelism)
Divide-se o fluxo de trabalho em funções ou etapas lógicas distintas.
* *Exemplo:* Em um simulador de voo, uma tarefa calcula a física e aerodinâmica, outra tarefa processa a renderização gráfica, e uma terceira gerencia o áudio e a entrada do piloto.

```
[INDICAÇÃO DE IMAGEM: Diagrama do modelo de Particionamento de Foster]
Descrição visual: À esquerda, um grande domínio de dados continuo (uma matriz 2D). No meio, a decomposição de dados dividindo a matriz em uma grade de milhares de pequenos quadrados de 1 pixel (Particionamento de Domínio). À direita, um fluxo de funções dividindo o algoritmo em etapas lógicas separadas (Particionamento Funcional).
```

---

### 2. Comunicação (*Communication*)

Uma vez dividido o problema em milhares de pequenas tarefas, precisamos determinar **quais dados precisam ser trocados entre essas tarefas** para que o cálculo global seja correto.

A comunicação introduz *overhead* (atraso e consumo de recursos). Portanto, devemos analisar os seguintes aspectos:

* **Comunicação Local vs. Global:**
  * *Local:* Uma tarefa só precisa de dados dos seus vizinhos imediatos (ex: calcular o brilho de um pixel baseado nos 4 pixels adjacentes).
  * *Global:* Uma tarefa precisa de dados de todas as outras tarefas do sistema (ex: calcular a média global da temperatura de toda a matriz).
* **Estruturada vs. Não-Estruturada:**
  * *Estruturada:* Os padrões de vizinhança formam grades, matrizes ou anéis previsíveis.
  * *Não-Estruturada:* A comunicação ocorre em grafos complexos ou malhas dinâmicas (ex: redes sociais ou simulação de fluidos adaptativa).
* **Padrão clássico — Troca de Fronteiras (*Halo Exchange / Ghost Cells*):**
  Em simulações numéricas de grade (como previsão do tempo ou transferência de calor), cada bloco de dados precisa apenas de uma "borda" (células fantasmas ou *halo*) das fatias vizinhas a cada iteração.

```
+-------------------------------------------------------------------------+
|                  PADRÃO DE COMUNICAÇÃO: HALO EXCHANGE                   |
+-------------------------------------------------------------------------+
|                                                                         |
|      Nó A (Processando Bloco A)        Nó B (Processando Bloco B)       |
|     +-------------------------+       +-------------------------+       |
|     | Dados  | Dados  | Halo  | <---> | Halo   | Dados  | Dados  |       |
|     | Locais | Locais | Env.A | Redes | Rec.A  | Locais | Locais |       |
|     +-------------------------+       +-------------------------+       |
|                         ▲                    ▲                          |
|                         +-- Troca Apenas --+                            |
|                             as Fronteiras                               |
+-------------------------------------------------------------------------+
```

---

### 3. Aglomeração (*Agglomeration*)

No mundo real, executar milhões de tarefas microscópicas é ineficiente. A criação, o escalonamento e a troca de mensagens para cada pequena tarefa geram um custo de gerenciamento (*overhead*) maior do que o tempo do próprio cálculo.

A **Aglomeração** consiste em **agrupar tarefas finas em blocos maiores** (transição de granularidade fina para granularidade grossa — *coarse-grained tasks*).

#### Objetivos da Aglomeração:
1. **Aumentar a Razão Computação/Comunicação:** Garantir que o tempo gasto calculando seja ordens de grandeza maior do que o tempo gasto comunicando.
2. **Preservar a Localidade de Dados:** Ao juntar dados vizinhos no mesmo bloco, aproveitamos as memórias cache L1/L2/L3.
3. **Reduzir o Custo de Software:** Menos *threads* ou mensagens criadas significa menos sobrecarga no Sistema Operacional e no hardware.

```
[INDICAÇÃO DE IMAGEM: Comparação de Granularidade de Foster]
Descrição visual: Ilustração da evolução da aglomeração. À esquerda: Granularidade Fina (milhares de pequenos blocos com uma teia densa e confusa de linhas de comunicação). À direita: Granularidade Grossa (pequenos blocos agrupados em 4 grandes regiões retangulares, resultando em pouquíssimas linhas de comunicação apenas nas bordas dos 4 blocos).
```

---

### 4. Mapeamento (*Mapping*)

Na última etapa, finalmente olhamos para a arquitetura física real e decidimos **qual nó computacional, soquete ou núcleo de CPU executará cada tarefa aglomerada**.

Os dois grandes objetivos do mapeamento são concorrentes e entram em conflito direto:
1. **Maximizar o Paralelismo e o Balanceamento de Carga (*Load Balancing*):** Manter todos os núcleos ocupados 100% do tempo.
2. **Minimizar o Tempo de Comunicação:** Colocar tarefas que se comunicam intensamente no mesmo nó ou em núcleos que compartilham a mesma cache L3 / nó NUMA.

#### Estratégias de Mapeamento:

* **Mapeamento Estático:** As tarefas são atribuídas aos processadores no início da execução e não mudam mais.
  * *Ideal para:* Cargas de trabalho previsíveis e homogêneas (ex: multiplicar matrizes de tamanho fixo).
  * *Técnicas:* Divisão em blocos contínuos (*Block Distribution*) ou intercalada (*Cyclic / Round-Robin Distribution*).
* **Mapeamento Dinâmico (Fila de Tarefas / *Work Stealing*):** As tarefas são colocadas em uma fila global ou distribuídas conforme os núcleos ficam livres.
  * *Ideal para:* Cargas de trabalho imprevisíveis ou heterogêneas (ex: renderização de cenas 3D onde algumas partes da imagem têm poucos reflexos e outras exigem rastreamento de raios denso).

---

## Estudo de Caso Prático: Difusão Térmica em uma Placa 2D (Stencil 2D)

Para ver o modelo PCAM funcionando de ponta a ponta, vamos projetar a paralelização de uma simulação de **condução de calor em uma placa metálica 2D**. A temperatura de cada ponto $(i, j)$ no tempo $t+1$ é a média dos seus 4 vizinhos imediatos no tempo $t$:

$$T_{new}[i][j] = 0.25 	imes (T[i-1][j] + T[i+1][j] + T[i][j-1] + T[i][j+1])$$

### Aplicação do Método PCAM ao Problema:

1. **Particionamento:** Cada ponto $(i, j)$ da placa de $N 	imes N$ é tratado como uma tarefa fina de cálculo de temperatura.
2. **Comunicação:** Para atualizar a célula $(i, j)$, a tarefa precisa ler os valores das células superior, inferior, esquerda e direita.
3. **Aglomeração:** Dividimos a matriz $N 	imes N$ em **faixas horizontais contíguas** de linhas (blocos de linhas). Assim, a comunicação interna entre células do mesmo bloco é eliminada, restando apenas a troca de 1 linha de borda (*halo*) com o bloco de cima e 1 com o de baixo.
4. **Mapeamento:** Mapeamos cada faixa horizontal para um *thread* OpenMP com afinidade de CPU estática (`schedule(static)`).

---

### Código em C (Implementação Prática com OpenMP)

```c
#include <stdio.h>
#include <stdlib.h>
#include <omp.h>

#define N 1000        // Tamanho da grade (1000x1000)
#define STEPS 100     // Passos de tempo da simulação

void inicializar_placa(float T[N][N]) {
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            // Fonte de calor no centro da placa
            if (i >= N/4 && i <= 3*N/4 && j >= N/4 && j <= 3*N/4)
                T[i][j] = 100.0f;
            else
                T[i][j] = 0.0f;
        }
    }
}

int main() {
    // Alocação das matrizes de temperatura
    float (*T)[N] = malloc(sizeof(float[N][N]));
    float (*T_new)[N] = malloc(sizeof(float[N][N]));

    inicializar_placa(T);

    double t_inicio = omp_get_wtime();

    // Loop de tempo da simulação
    for (int step = 0; step < STEPS; step++) {
        
        // --- AGLOMERAÇÃO E MAPEAMENTO VIA OPENMP ---
        // As linhas de 1 a N-2 são divididas em blocos estáticos entre as threads.
        // A comunicação de bordas (halo) é tratada pela memória compartilhada.
        #pragma omp parallel for schedule(static) collapse(2)
        for (int i = 1; i < N - 1; i++) {
            for (int j = 1; j < N - 1; j++) {
                T_new[i][j] = 0.25f * (T[i-1][j] + T[i+1][j] + 
                                       T[i][j-1] + T[i][j+1]);
            }
        }

        // Troca de ponteiros (atualização do estado da placa)
        #pragma omp single
        {
            float (*temp)[N] = T;
            T = T_new;
            T_new = temp;
        }
    }

    double t_fim = omp_get_wtime();

    printf("Simulação concluída com sucesso em %.4f segundos.
", t_fim - t_inicio);
    printf("Temperatura no centro (N/2, N/2): %.2f °C
", T[N/2][N/2]);

    free(T);
    free(T_new);
    return 0;
}
```

---

## Tabela Resumo: O Ciclo de Vida do Projeto Paralelo (PCAM)

| Etapa | Pergunta-Chave | Ação Prática | Foco |
| :--- | :--- | :--- | :--- |
| **1. Particionamento** | *Onde está o paralelismo oculto?* | Dividir o problema no maior número de micro-tarefas. | **Exposição Máxima** |
| **2. Comunicação** | *Quais dados as tarefas trocam?* | Mapear o fluxo e os padrões de dados entre tarefas. | **Estruturação do Fluxo** |
| **3. Aglomeração** | *Como juntar tarefas com eficiência?* | Agrupar micro-tarefas para reduzir overhead. | **Razão Computação/Comunicação** |
| **4. Mapeamento** | *Quem vai rodar o quê no hardware?* | Atribuir blocos a núcleos/nós balanceando a carga. | **Desempenho e Localidade** |

---

## Exercícios de Fixação e Reflexão

1. **Por que a etapa de Particionamento do Modelo de Foster exige que ignoremos o número de processadores reais do sistema?**
2. **Explique a diferença entre Mapeamento Estático e Mapeamento Dinâmico. Dê um exemplo de algoritmo onde o mapeamento estático resulta em um grave desbalanceamento de carga.**
3. **Em uma simulação de grade 2D, por que aglomerar células em blocos retangulares/quadrados gera menos overhead de comunicação do que aglomerá-las em linhas longas e finas?** *(Dica: Pense na razão entre Perímetro e Área).*

---

Com este método bem estabelecido, encerramos a introdução teórica do projeto de softwares paralelos!
