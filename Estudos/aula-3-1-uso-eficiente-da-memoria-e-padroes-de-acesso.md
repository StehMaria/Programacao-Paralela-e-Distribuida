# Aula 3.1: Uso Eficiente da Memória e Padrões de Acesso — Localidade, Unit Stride vs. Non-Unit Stride

---

## 1. Introdução: A Física da Memória e o Custo do "Cache Miss"

Nas aulas anteriores, vimos que a CPU moderna é uma verdadeira "fórmula 1" do processamento, enquanto a memória RAM principal funciona como um "caminhão pesado". Quando o núcleo da CPU precisa de um dado e esse dado já está dentro da **Memória Cache (L1, L2 ou L3)**, a resposta é quase instantânea (poucos ciclos de clock). Por outro lado, se o dado precisa ser buscado lá na **RAM**, a CPU é forçada a parar tudo e esperar centenas de ciclos de clock — um evento conhecido como **Cache Miss** (falta na cache).

Mas como o hardware decide o que deve ficar na memória cache?

A resposta é simples: **o hardware não carrega bytes isolados da RAM; ele carrega blocos contínuos de dados chamados Linhas de Cache (Cache Lines)**, que na grande maioria das arquiteturas modernas (x86_64 e ARM) possuem exatamente **64 bytes**.

Se você pede 1 único número inteiro de 4 bytes (`int`), o processador traz esse inteiro **junto com os 60 bytes vizinhos** que estão ao lado dele na memória RAM. 

A eficiência do seu programa depende quase inteiramente de como você aproveita esses outros 60 bytes "carona" que vieram na mesma linha de cache!

---

## 2. Analogia Central: A Biblioteca e a Caixa de Livros

Pense na memória RAM como uma **biblioteca central gigantesca** no outro lado da cidade, e na Memória Cache L1 como uma **pequena mesa de estudo** ao seu lado.

1. **Sem aproveitar a linha de cache:** Você precisa ler a página 1 do Livro A. Você vai à biblioteca, pega o livro, traz para a mesa e lê a página 1. Depois, precisa da página 2. Em vez de abrir o mesmo livro que já está na mesa, você devolve o livro, vai até a biblioteca e pega o Livro B. Em seguida devolve, vai à biblioteca e pega o Livro C. Você passou 99% do tempo caminhando e 1% lendo!
2. **Aproveitando a linha de cache (*Unit Stride*):** Quando você vai à biblioteca buscar o Livro A, o bibliotecário entrega uma **caixa contendo os Livros A, B, C, D, E e F** (a Linha de Cache de 64 bytes). Se o seu trabalho exige ler a sequência exata de A até F, você pega o Livro A, depois lê o B que já está na caixa da sua mesa, depois o C, o D, o E e o F. Apenas quando precisar do Livro G você fará uma nova viagem à biblioteca.

> **Regra de Ouro:** Fazer uma viagem à RAM para usar apenas 4 bytes da linha de cache de 64 bytes é desperdiçar 93.75% da banda de memória e da capacidade do sistema.

---

## 3. Os Dois Princípios da Localidade de Memória

Toda a arquitetura de computadores de alto desempenho se apoia em dois princípios comportamentais dos programas:

```
                          ┌────────────────────────────────────────────────────────┐
                          │            PRINCÍPIOS DA LOCALIDADE                    │
                          └───────────────────────────┬────────────────────────────┘
                                                      │
                       ┌──────────────────────────────┴──────────────────────────────┐
                       ▼                                                             ▼
         ┌───────────────────────────┐                                 ┌───────────────────────────┐
         │    LOCALIDADE TEMPORAL    │                                 │    LOCALIDADE ESPACIAL    │
         ├───────────────────────────┤                                 ├───────────────────────────┤
         │ Se um dado foi acessado   │                                 │ Se um dado foi acessado   │
         │ agora, ele provavelmente  │                                 │ agora, seus VIZINHOS de   │
         │ será acessado NOVAMENTE   │                                 │ memória serão acessados   │
         │ em um futuro próximo.     │                                 │ em breve.                 │
         │                           │                                 │                           │
         │ Exemplo: Variáveis de     │                                 │ Exemplo: Percorrer um     │
         │ contagem em laços (i++).  │                                 │ vetor elemento por        │
         └───────────────────────────┘                                 │ elemento (A[0], A[1]...). │
                                                                       └───────────────────────────┘
```

---

## 4. Padrões de Acesso: Unit Stride vs. Non-Unit Stride

A forma como seu código percorre estruturas de dados (vetores, matrizes, grafos) define o **Passo de Acesso (*Stride*)**:

### A) Acesso Contíguo (*Unit Stride* ou *Stride-1*)
É o padrão ideal. O programa acessa posições de memória consecutivas: `A[0]`, `A[1]`, `A[2]`, `A[3]`...

```
Endereço RAM:   [ 0x00 ] [ 0x04 ] [ 0x08 ] [ 0x0C ] [ 0x10 ] [ 0x14 ] ...
Vetor A:        │ A[0]  │ A[1]  │ A[2]  │ A[3]  │ A[4]  │ A[5]  │
Passo (Stride):  ───────►───────►───────►───────►───────►
```
* **Comportamento na Cache:** A primeira leitura (`A[0]`) gera um **Cache Miss**, trazendo uma linha de 64 bytes contendo de `A[0]` a `A[15]` (assumindo inteiros de 4 bytes). As 15 leituras seguintes (`A[1]` até `A[15]`) resultam em **Cache Hits** instantâneos!
* **Taxa de Acerto (*Hit Rate*):** ~93.75%.

---

### B) Acesso Saltado (*Non-Unit Stride* ou *Stride-N*)
O programa pula posições de memória a cada iteração: `A[0]`, `A[16]`, `A[32]`...

```
Endereço RAM:   [ 0x00 ] [ 0x04 ] ... [ 0x40 ] ... [ 0x80 ] ...
Vetor A:        │ A[0]  │ ...   │ ... │ A[16] │ ...│ A[32] │
Passo (Stride):  ────────────────────►────────────────────►
```
* **Comportamento na Cache:** A leitura de `A[0]` carrega a linha de 64 bytes contendo `A[0]` a `A[15]`. Em seguida, o código pula para `A[16]`, que está na linha seguinte. **Toda iteração gera um Cache Miss!** Os outros 60 bytes trazidos para a cache são descartados sem nunca terem sido lidos.
* **Taxa de Acerto (*Hit Rate*):** ~0%.

---

### C) Acesso Randômico ou Indireto (*Pointer Chasing*)
Ocorrem em estruturas encadeadas (listas ligadas, árvores, grafos) ou indexação indireta (`A[B[i]]`).
O endereço da próxima leitura depende do valor armazenado no nó atual. A CPU não consegue prever para onde o programa vai e sofre sucessivos atrasos de memória.

---

## 5. Mapeamento de Matrizes: Row-Major vs. Column-Major Order

A memória RAM é estritamente **unidimensional** (uma longa fila de endereços numerados de 0 a $N$). Para armazenar uma matriz bidimensional (linhas e colunas) em uma RAM unidimensional, a linguagem de programação precisa escolher uma convenção de organização.

```
                  ┌──────────────────────────────────────────────┐
                  │ MATRIZ2D (2 linhas x 3 colunas)             │
                  │                                              │
                  │            Coluna 0   Coluna 1   Coluna 2    │
                  │ Linha 0  │   A00   │   A01   │   A02   │     │
                  │ Linha 1  │   A10   │   A11   │   A12   │     │
                  └──────────────────────────────────────────────┘
                                         │
                   ┌─────────────────────┴─────────────────────┐
                   ▼                                           ▼
┌───────────────────────────────────────┐   ┌───────────────────────────────────────┐
│     ROW-MAJOR ORDER (C, C++, Python)  │   │   COLUMN-MAJOR ORDER (Fortran, MATLAB)│
├───────────────────────────────────────┤   ├───────────────────────────────────────┤
│ Armazena LINHA por LINHA contígua.    │   │ Armazena COLUNA por COLUNA contígua.  │
│                                       │   │                                       │
│ [ A00 ][ A01 ][ A02 ][ A10 ][ A11 ].. │   │ [ A00 ][ A10 ][ A01 ][ A11 ][ A02 ].. │
└───────────────────────────────────────┘   └───────────────────────────────────────┘
```

### O Erro Clássico em Linguagem C
Em C/C++, a matriz é armazenada em **Row-Major Order**. Os elementos de uma mesma linha (`A[i][0]`, `A[i][1]`, `A[i][2]`) estão fisicamente lado a lado na memória RAM.

* **Percorrer por LINHA (Correto - Unit Stride):**
  ```c
  // Laço externo = Linha, Laço interno = Coluna
  for (int i = 0; i < N; i++) {
      for (int j = 0; j < N; j++) {
          soma += A[i][j]; // Acessa A[i][0], A[i][1], A[i][2]... (CONTÍGUO!)
      }
  }
  ```

* **Percorrer por COLUNA (Incorreto - Non-Unit Stride / Stride-N):**
  ```c
  // Laço externo = Coluna, Laço interno = Linha
  for (int j = 0; j < N; j++) {
      for (int i = 0; i < N; i++) {
          soma += A[i][j]; // Acessa A[0][j], A[1][j], A[2][j]... (SALTADO!)
      }
  }
  ```
  A cada passo de `i`, a CPU precisa saltar $N 	imes 	ext{sizeof(int)}$ bytes na RAM, destruindo completamente o aproveitamento da linha de cache!

---

## 6. O Pré-carregador de Hardware (*Hardware Prefetcher*)

Processadores modernos possuem um circuito interno chamado **Hardware Prefetcher**.

Se o pré-carregador detecta que seu código está acessando a memória em um padrão previsível e contíguo (**Unit Stride**), ele antecipa os próximos acessos e **busca as próximas linhas de cache na RAM antes mesmo da CPU pedir!**

Com o *Prefetcher* ativo em um código com *Unit Stride*, a latência aparente da RAM cai quase a zero, pois os dados chegam na Cache L1 um instante antes de serem calculados. Se você usar *Non-Unit Stride* ou acessos randômicos, o *Prefetcher* se confunde, desliga-se, e seu código paga a penalidade total da latência da RAM.

---

## 7. Exemplo Prático em C: Comparando Desempenho Real

Abaixo está um código completo em C que mede a diferença brutal de tempo entre percorrer uma matriz $4000 	imes 4000$ por linhas (*Unit Stride*) versus por colunas (*Non-Unit Stride*).

```c
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define N 4000

// Matriz grande alocada globalmente (ou no Heap) para evitar estouro de pilha
double A[N][N];

int main() {
    // Inicializa a matriz
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            A[i][j] = 1.0;
        }
    }

    struct timespec start, end;
    double soma = 0.0;

    // =========================================================================
    // TESTE 1: Acesso por Linha (Row-Major / Unit Stride - CORRETO)
    // =========================================================================
    clock_gettime(CLOCK_MONOTONIC, &start);

    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            soma += A[i][j];
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    double tempo_linha = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    printf("[UNIT STRIDE]     Acesso por Linhas:  %f segundos (Soma: %.1f)\n", tempo_linha, soma);

    // =========================================================================
    // TESTE 2: Acesso por Coluna (Column-Major / Non-Unit Stride - INCORRETO)
    // =========================================================================
    soma = 0.0;
    clock_gettime(CLOCK_MONOTONIC, &start);

    for (int j = 0; j < N; j++) {
        for (int i = 0; i < N; i++) {
            soma += A[i][j];
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &end);
    double tempo_coluna = (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec) / 1e9;
    printf("[NON-UNIT STRIDE] Acesso por Colunas: %f segundos (Soma: %.1f)\n", tempo_coluna, soma);

    printf("\n--> O acesso por colunas foi %.2fx MAIS LENTO devido a Cache Misses!\n", tempo_coluna / tempo_linha);

    return 0;
}
```

### Resultado Típico de Execução num Servidor x86_64:
```text
[UNIT STRIDE]     Acesso por Linhas:  0.018240 segundos (Soma: 16000000.0)
[NON-UNIT STRIDE] Acesso por Colunas: 0.142810 segundos (Soma: 16000000.0)

--> O acesso por colunas foi 7.83x MAIS LENTO devido a Cache Misses!
```
*Ambos os laços executam exatamente as mesmas 16 milhões de somas, mas a versão que respeita o layout da memória é quase **8 vezes mais rápida** simplesmente por usar bem as linhas de cache!*

---

## 8. Tabela Comparativa de Padrões de Acesso

| Padrão | Descrição | Taxa de Acerto de Cache | Hardware Prefetcher | Desempenho |
| :--- | :--- | :--- | :--- | :--- |
| **Unit Stride (Stride-1)** | Elementos consecutivos em memória (`A[i]`, `A[i+1]`) | **Excelente (~93-100%)** | Atua perfeitamente | **Máximo** |
| **Small Stride (Stride-2/4)** | Salta poucos elementos (`A[i]`, `A[i+2]`) | Moderada (~50-75%) | Consegue acompanhar | Médio |
| **Large Stride (Stride-N)** | Salta linhas inteiras de matrizes grandes | **Péssima (~0%)** | Falha / Desliga | **Muito Ruim** |
| **Indirect / Random** | Indexação por ponteiro/vetor (`A[B[i]]`) | Péssima / Imprevisível | Inútil | **Crítico** |

---

## 9. Exercícios de Fixação

1. **Por que percorrer uma matriz em C por colunas gera uma taxa de acerto de cache próxima de 0% se a matriz for grande?**
2. **Qual é a diferença de layout de memória entre a linguagem C e a linguagem Fortran ao declarar uma matriz bidimensional?**
3. **O que faz o Hardware Prefetcher e por que ele precisa do padrão Unit Stride para operar com máxima eficiência?**
4. **Em um vetor de estruturas `struct Ponto { float x, y, z; } pontos[1000]`, como a leitura apenas da coordenada `x` de todos os pontos se comporta em termos de Stride?**

---

*Próxima Aula: **Aula 3.2 — Transformações e Reestruturação de Laços (Loop Tiling / Blocking, Unrolling e Loop Interchange)**!*
