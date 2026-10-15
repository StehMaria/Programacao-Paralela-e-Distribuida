# Aula 4.4: Modelos Abstratos de Computação — O Modelo PRAM (EREW, CREW, CRCW) e Análise Assintótica

## Introdução: Por que Precisamos de Modelos Abstratos?

Ao projetar algoritmos para computadores tradicionais de um único núcleo, utilizamos o modelo **RAM (*Random Access Machine*)**. Nesse modelo simplificado, assumimos que:
1. As instruções são executadas em sequência, uma de cada vez.
2. Qualquer posição de memória pode ser lida ou escrita em **tempo constante $O(1)$**.
3. Não há latência de rede, estouro de cache ou concorrência.

Embora um computador real x86 ou ARM tenha hierarquia de cache, paginação de memória e arquiteturas complexas, o modelo RAM é incrivelmente útil porque **isola a lógica do algoritmo da complexidade do hardware**. Ele nos permite calcular se um algoritmo é $O(n)$, $O(n \log n)$ ou $O(n^2)$ sem nos preocuparmos com a marca do processador.

Na computação paralela, o equivalente ao modelo RAM é o **PRAM (*Parallel Random Access Machine*)**. Trata-se do modelo teórico mais importante para o projeto e a análise matemática de algoritmos paralelos.

---

## 1. O que é o Modelo PRAM?

O modelo PRAM imagina um supercomputador idealizado formado por:
* **$P$ Processadores idênticos** ($P_0, P_1, \dots, P_{P-1}$), cada um com seus registradores locais.
* **Uma Memória Compartilhada Global Ilimitada**, acessível por todos os processadores.
* **Sincronismo Estreito por Clock Comum:** Em cada unidade de tempo (passo de clock), todos os processadores executam uma instrução em paralelo (ler da memória, fazer uma operação aritmética/lógica em registradores locais ou escrever na memória).

```
+-----------------------------------------------------------------------+
|                   MEMÓRIA COMPARTILHADA GLOBAL                        |
|   [ M[0] ]    [ M[1] ]    [ M[2] ]    [ M[3] ]   ...   [ M[N-1] ]     |
+-----------------------------------------------------------------------+
       ^           ^           ^           ^                 ^
       |           |           |           |                 |
   +-------+   +-------+   +-------+   +-------+         +-------+
   |  P_0  |   |  P_1  |   |  P_2  |   |  P_3  |   ...   | P_P-1 |  (Processadores)
   +-------+   +-------+   +-------+   +-------+         +-------+
       |           |           |           |                 |
 +------------------------------------------------------------------+
 |              CLOCK SIMULTÂNEO E PASSO A PASSO                    |
 +------------------------------------------------------------------+
```
> **Indicação de Imagem / Arquivo Visual:** `figura_modelo_pram_arquitetura.png` — Diagrama mostrando múltiplos processadores conectados por um barramento ideal sem conflito a uma memória global unificada sob um clock comum.

### A Analogia do Quadro Negro na Sala de Aula
Pense em uma sala de aula onde há **$P$ alunos (processadores)** trabalhando juntos e um gigante **quadro negro (memória compartilhada)** na parede.
* Cada aluno tem um pedaço de rascunho na mesa (registradores locais).
* O professor bate a régua na mesa a cada segundo (**clock sincrono**).
* A cada batida, todos os alunos podem se levantar ao mesmo tempo, olhar para uma posição do quadro, anotar um número no seu rascunho, fazer uma conta de cabeça e escrever o resultado de volta no quadro.

---

## 2. Variações do PRAM: Conflitos de Acesso à Memória

A grande pergunta no modelo PRAM é: **O que acontece se dois ou mais processadores tentarem ler ou escrever na MESMA posição da memória compartilhada EXATAMENTE no mesmo ciclo de clock?**

Dependendo da resposta, dividimos o modelo PRAM em 4 subcategorias principais, indo da mais restritiva à mais permissiva.

```
                  +-----------------------------------+
                  |        Modelos PRAM               |
                  +-----------------------------------+
                                    |
          +-------------------------+-------------------------+
          |                                                   |
   Sem Conflitos na Escrita                             Com Conflitos na Escrita
   (Exclusive Write - EW)                               (Concurrent Write - CW)
          |                                                   |
    +-----+-----+                                       +-----+-----+
    |           |                                       |           |
  EREW        CREW                                    ERCW        CRCW
 (Mais      (Mais                                    (Raro)    (Mais Poderoso)
 Rígido)     Comum)                                                  |
                                                        +------------+------------+
                                                        |            |            |
                                                     Common      Arbitrary     Priority
```

---

### 2.1 EREW (*Exclusive Read, Exclusive Write*)
* **Regra:** **Nenhum** conflito é permitido. Em qualquer ciclo de clock, duas ou mais threads **não podem** ler a mesma posição de memória nem escrever na mesma posição de memória.
* **Analogia:** Dois alunos jamais podem olhar para a mesma palavra no quadro negro ao mesmo tempo, nem tentar escrever na mesma célula.
* **Realismo:** É o modelo PRAM mais próximo do hardware real de baixo nível (pois simula memórias sem suporte a broadcast ou resolução de colisões).

### 2.2 CREW (*Concurrent Read, Exclusive Write*)
* **Regra:** **Múltiplos** processadores podem **LER** da mesma posição de memória simultaneamente. No entanto, a **ESCRITA** continua sendo estritamente exclusiva (apenas 1 processador por célula por ciclo).
* **Analogia:** Todos os alunos podem ler o enunciado do problema escrito no topo do quadro simultaneamente, mas apenas um aluno por vez pode escrever a resposta em uma caixinha.
* **Uso:** É o modelo mais utilizado no projeto de algoritmos teóricos, pois "leitura simultânea" é fácil de conceituar.

### 2.3 ERCW (*Exclusive Read, Concurrent Write*)
* **Regra:** Leituras simultâneas são proibidas, mas escritas simultâneas são permitidas.
* **Status:** Praticamente **teórico/acadêmico**, sem aplicação prática relevante.

### 2.4 CRCW (*Concurrent Read, Concurrent Write*)
* **Regra:** Tanto leituras simultâneas quanto escritas simultâneas são permitidas na mesma posição de memória no mesmo ciclo de clock.
* **Poder:** É a variação mais poderosa do PRAM. Ela consegue resolver problemas em menos passos teóricos.

#### Como Resolver Conflitos de Escrita no CRCW?
Quando 5 processadores tentam escrever valores diferentes na variável `M[10]` exatamente no mesmo instante, o modelo precisa ter uma regra de desempate:

1. **Common CRCW:** Todos os processadores que tentam escrever simultaneamente na mesma posição **devem escrever exatamente o mesmo valor**. Se tentarem escrever valores diferentes, ocorre um erro/falha no sistema.
2. **Arbitrary CRCW:** O sistema escolhe aleatoriamente **um único processador vencedor** para escrever seu valor. Os outros são ignorados.
3. **Priority CRCW:** O processador com a **menor (ou maior) identificação ($ID$)** vence a disputa e grava seu valor na memória.
4. **Combining CRCW:** Os valores gravados por todos os processadores concorrentes são combinados usando uma operação associativa atômica (Soma, Máximo, Mínimo, AND lógico). Por exemplo, se $P_1$ escreve 5 e $P_2$ escreve 3 em `M[0]`, a célula recebe $5 + 3 = 8$.

---

## 3. Análise Assintótica de Algoritmos Paralelos

Para avaliar a qualidade de um algoritmo paralelo no modelo PRAM, não medimos apenas o tempo $T$. Analisamos um conjunto de métricas formais:

### 3.1 Tempo Paralelo ($T(n, p)$ ou $T_p$)
É o número de ciclos de clock (passos de tempo) desde o início até a conclusão do algoritmo usando $p$ processadores para um problema de tamanho $n$.

### 3.2 Trabalho ($W(n)$)
É o número **total de operações individuais** realizadas por todos os processadores durante a execução:
$$W(n) = \sum_{t=1}^{T_p} p_t$$
Onde $p_t$ é o número de processadores ativos no passo de tempo $t$.

> **Princípio Importante:** Um algoritmo paralelo eficiente deve realizar um trabalho total $W(n)$ da mesma ordem de grandeza do melhor algoritmo sequencial $T_{	ext{seq}}(n)$, ou seja, $W(n) = \Theta(T_{	ext{seq}}(n))$.

### 3.3 Custo ($C(n, p)$)
É o produto do tempo paralelo pelo número total de processadores alocados:
$$C(n, p) = p 	imes T(n, p)$$

### 3.4 Otimalidade de Custo (*Cost-Optimality*)
Um algoritmo paralelo é dito **Custo-Ótimo** se o seu Custo $C(n, p)$ for assintoticamente igual ao tempo do melhor algoritmo sequencial conhecido para resolver o mesmo problema:
$$C(n, p) = \Theta(T_{	ext{seq}}(n))$$

Se um algoritmo sequencial de ordenação leva $O(n \log n)$, e nosso algoritmo paralelo roda em $T(n, p) = O(\log n)$ usando $p = n$ processadores:
* Custo $C = n 	imes \log n = O(n \log n)$.
* Como o custo coincide com o tempo sequencial, o algoritmo é **Custo-Ótimo**!

---

## 4. Teorema de Brent (Princípio do Agendamento de Trabalhos)

O **Teorema de Brent** é um dos resultados mais fundamentais da computação paralela teórica. Ele responde à seguinte pergunta:

> *"Se eu desenhei um algoritmo paralelo que assume um número infinito de processadores e roda em tempo $T_\infty$ fazendo um trabalho total $W(n)$, quanto tempo ele levará se eu tiver apenas $p$ processadores reais?"*

### A Fórmula do Teorema de Brent:
$$T_p \le rac{W(n)}{p} + T_\infty$$

Onde:
* $T_p$: Tempo de execução com $p$ processadores.
* $W(n)$: Trabalho total (total de operações).
* $T_\infty$: Tempo de execução com infinitos processadores (comprimento do **caminho crítico** ou profundidade do circuito).

### O que o Teorema de Brent nos ensina?
1. Se $p$ for muito grande ($p 	o \infty$), o termo $rac{W(n)}{p}$ tende a zero e $T_p pprox T_\infty$ (atingimos o limite físico do algoritmo).
2. Se $p$ for pequeno, a execução é dominada pela divisão do trabalho total pelo número de processadores ($rac{W(n)}{p}$).
3. **Simulação de Hardware:** Não precisamos reescrever o algoritmo do zero para cada quantidade de processadores. O Teorema de Brent garante que podemos mapear qualquer algoritmo desenhado para $N$ processadores em um sistema com $p < N$ processadores com perda mínima de eficiência.

---

## 5. Exemplo Algorítmico Prático: Redução Paralela (Soma de um Vetor)

Para ver o modelo PRAM em ação, vamos analisar a **Soma de $N$ elementos de um vetor**.

### Algoritmo Sequencial Tradicional
* Percorre o vetor de $N$ elementos com um laço `for`.
* Tempo: $T_{	ext{seq}}(n) = O(n)$.
* Trabalho: $W(n) = O(n)$.

### Algoritmo PRAM no Modelo CREW / EREW (Árvore Binária de Redução)
Em vez de somar sequencialmente, formamos uma árvore binária de somas em $k = \log_2 n$ etapas.

* **Passo 1:** $P_0$ soma $V[0]+V[1]$, $P_1$ soma $V[2]+V[3]$, $P_2$ soma $V[4]+V[5]$, etc. (Soma $N/2$ pares em 1 ciclo).
* **Passo 2:** Os resultados são somados em pares novamente (Soma $N/4$ pares em 1 ciclo).
* **Passo $\log_2 N$:** Resta apenas 1 elemento no topo da árvore com a soma total.

```
Passo 0 (Início):  [ 3 ]  [ 5 ]  [ 2 ]  [ 8 ]  [ 1 ]  [ 4 ]  [ 7 ]  [ 6 ]   (N = 8)
                     \    /        \    /        \    /        \    /
Passo 1 (t = 1):     [  8  ]        [ 10  ]        [  5  ]        [ 13  ]   (4 somas paralelas)
                        \            /               \            /
Passo 2 (t = 2):        [    18     ]                 [    18     ]         (2 somas paralelas)
                            \                       /
Passo 3 (t = 3):            [          36          ]                   (1 soma final)
```
> **Indicação de Imagem / Arquivo Visual:** `figura_reducao_arvore_binaria_pram.png` — Árvore de redução paralela mostrando a redução de $N$ elementos para 1 em $\log_2 N$ passos.

### Análise de Complexidade do Algoritmo Paralelo:
* **Tempo Paralelo ($T_p$):** $\log_2 n$ passos $	o T(n, p) = O(\log n)$.
* **Trabalho ($W(n)$):** $rac{n}{2} + rac{n}{4} + \dots + 1 = n - 1 = O(n)$.
* **Custo ($C(n, p)$ com $p = n/2$):** $rac{n}{2} 	imes O(\log n) = O(n \log n)$.
* **Análise de Otimalidade:**
  * O algoritmo com $p = n/2$ não é perfeitamente custo-ótimo porque $O(n \log n) 
eq O(n)$.
  * **Aplicação do Teorema de Brent:** Se usarmos apenas $p = rac{n}{\log n}$ processadores e cada um somar localmente uma fatia de $\log n$ elementos sequencialmente antes da árvore, o tempo será $T_p = O(\log n)$ e o Custo será $C = \left(rac{n}{\log n}ight) 	imes O(\log n) = O(n)$ $	o$ **Tornando o algoritmo CUSTO-ÓTIMO!**

---

### Código Prático em C Simulando o Modelo PRAM de Redução com OpenMP

Abaixo temos uma implementação em C que simula o algoritmo de árvore de redução binária estilo PRAM usando a API de barreiras e threads para evidenciar a divisão em passos de tempo sincronizados:

```c
#include <stdio.h>
#include <stdlib.h>
#include <omp.h>
#include <math.h>

#define N 16  // Tamanho do vetor (Deve ser potencia de 2 para simplificar)

int main() {
    int A[N] = {3, 5, 2, 8, 1, 4, 7, 6, 9, 12, 15, 2, 11, 5, 8, 4};
    int num_threads = N / 2;

    printf("=== SIMULADOR DE ALGORITMO PRAM (REDUÇÃO EM ÁRVORE) ===\n");
    printf("Vetor inicial: ");
    for (int i = 0; i < N; i++) printf("%d ", A[i]);
    printf("\n\n");

    double start_time = omp_get_wtime();

    // Região paralela simulando o modelo PRAM sincrono
    #pragma omp parallel num_threads(num_threads)
    {
        int tid = omp_get_thread_num();
        
        // k é o número de etapas da árvore (log2(N))
        int passos = (int)log2(N);

        for (int d = 0; d < passos; d++) {
            int stride = 1 << d; // 2^d

            // Apenas threads ativas nesta etapa realizam o trabalho
            if (tid % stride == 0 && (2 * tid + stride) < N) {
                int idx_destino = 2 * tid;
                int idx_origem  = 2 * tid + stride;

                // Leitura e Escrita Sincronizada PRAM
                A[idx_destino] = A[idx_destino] + A[idx_origem];
            }

            // BARREIRA PRAM: Garante que o passo de clock 'd' terminou para TODOS
            #pragma omp barrier

            // Apenas a thread 0 imprime o estado do vetor após o passo de clock
            #pragma omp single
            {
                printf("[Passo de Clock %d terminado] Vetor parcialmente somado: ", d + 1);
                for (int i = 0; i < N; i++) printf("%d ", A[i]);
                printf("\n");
            }
            #pragma omp barrier
        }
    }

    double end_time = omp_get_wtime();

    printf("\nSoma Total Calculada: %d\n", A[0]);
    printf("Tempo total de execução simulado: %f segundos\n", end_time - start_time);

    return 0;
}
```

---

## 6. Comparação Direta entre os Modelos PRAM

| Modelo PRAM | Leitura Concorrente | Escrita Concorrente | Complexidade de Hardware | Exemplo de Aplicação / Algoritmo |
| :--- | :---: | :---: | :--- | :--- |
| **EREW** | ❌ Não | ❌ Não | **Baixa** (Mais fácil de mapear em hardware real) | Redução em Árvore, Prefix Sum (*Scan*) |
| **CREW** | ✅ Sim | ❌ Não | **Média** | Pesquisa paralela em matrizes, Multiplicação de Matrizes |
| **ERCW** | ❌ Não | ✅ Sim | **Teórica** | Pouco utilizado na prática |
| **CRCW (Common)** | ✅ Sim | ✅ Sim (Igual) | **Alta** | Verificação de ordenação, Algoritmos em Grafos |
| **CRCW (Priority)**| ✅ Sim | ✅ Sim (Priorit.)| **Muito Alta** | Encontrar o Elemento Máximo em tempo $O(1)$ |

---

## 7. Limitações do Modelo PRAM no Mundo Real

Embora o PRAM seja indispensável na ciência da computação teórica, ele possui abstrações que **não existem** no hardware real de computadores:

1. **Ignora Latência de Memória:** O PRAM assume que qualquer acesso à RAM leva 1 ciclo de clock. Na vida real, acessar a RAM pode levar **200 ciclos de clock** se houver um *Cache Miss*.
2. **Ignora Largura de Banda e Contenção:** No PRAM, 1.000 processadores podem ler 1.000 variáveis diferentes da RAM simultaneamente sem nenhum gargalo no barramento. No hardware real, haverá contenção grave no barramento de memória.
3. **Custo de Sincronismo Perfeito:** Manter mil núcleos operando exatamente no mesmo pulso de clock (*Lock-step*) é inviável em escala devido ao desvio de clock (*Clock Skew*).
4. **Ausência de Caches:** O modelo PRAM não contempla a hierarquia de memória L1/L2/L3.

---

## Resumo da Aula e Conclusões

* O **modelo PRAM** é a ferramenta teórica padrão para projetar algoritmos paralelos, abstraindo detalhes de hardware.
* É classificado em **EREW, CREW, ERCW e CRCW** de acordo com a permissão de conflitos de leitura e escrita simultânea.
* O **Teorema de Brent** prova que podemos adaptar algoritmos desenhados para infinitos processadores para rodar em hardware real com $p$ processadores mantendo o desempenho quase ótimo.
* Algoritmos como a **Redução em Árvore Binária** reduzem problemas de tempo sequencial $O(n)$ para tempo paralelo $O(\log n)$.

---

## Exercícios de Fixação com Gabarito

### Exercício 1 (Conceitual)
**Qual é a diferença fundamental entre os modelos PRAM EREW e CREW? Dê um exemplo de operação que é permitida no CREW mas proibida no EREW.**
* **Gabarito:** No modelo **EREW** (*Exclusive Read, Exclusive Write*), duas threads não podem ler nem escrever na mesma posição de memória no mesmo ciclo. No **CREW** (*Concurrent Read, Exclusive Write*), múltiplas threads podem ler a mesma posição simultaneamente.
* *Exemplo de operação:* Ler o valor de uma variável global de configuração `int N = A[0];` por todas as 100 threads no mesmo passo de clock é permitido no CREW, mas proibido no EREW (no EREW essa transmissão exige uma árvore de broadcast de tempo $O(\log p)$).

---

### Exercício 2 (Cálculo com Teorema de Brent)
**Um algoritmo paralelo realiza um trabalho total $W(n) = 1000$ operações e tem um tempo de caminho crítico $T_\infty = 10$ passos. Usando o Teorema de Brent, qual é o tempo máximo estimado de execução $T_p$ se tivermos $p = 100$ processadores?**
* **Resolução:**
  $$\text{Pelo Teorema de Brent: } T_p \le \frac{W(n)}{p} + T_\infty$$
  $$T_p \le \frac{1000}{100} + 10$$
  $$T_p \le 10 + 10 = 20 \text{ passos.}$$
* **Resposta:** O tempo máximo estimado de execução será de **20 passos**.
