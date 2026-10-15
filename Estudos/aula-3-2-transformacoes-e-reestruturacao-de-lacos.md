# Aula 3.2: Transformações e Reestruturação de Laços — Loop Tiling, Unrolling e Interchange

## Introdução: O Papel da Reestruturação de Código
Na Aula 3.1, aprendemos que a memória RAM é organizada de forma contígua e que a CPU carrega dados em linhas de cache (*Cache Lines*) de 64 bytes. Vimos que acessar a memória de forma sequencial (**Unit Stride**) é essencial para o desempenho.

No entanto, em algoritmos mais complexos — como multiplicação de matrizes, processamento de imagens e simulações físicas —, laços aninhados simples frequentemente reusam dados de forma ineficiente ou estouram a capacidade da cache L1/L2.

A **Reestruturação de Laços (*Loop Transformations*)** é um conjunto de técnicas de otimização aplicadas tanto por compiladores modernos (`-O2`, `-O3`) quanto por programadores de alto desempenho para:
1. **Reorganizar a ordem de acesso à memória** para maximizar a taxa de acerto (*Hit Rate*) da cache.
2. **Reduzir o overhead de instruções de controle** (como saltos e comparações de laço).
3. **Expor Paralelismo em Nível de Instrução (ILP)** e preparar o código para **Vetorização SIMD**.

---

## 1. Loop Interchange (Troca de Laços)

### O Conceito
O **Loop Interchange** consiste em alterar a ordem de execução de dois ou mais laços `for` aninhados sem alterar o resultado semântico do algoritmo.

### Analogia do Livro de Receitas
Imagine que você tem uma pilha de 100 livros de receita. Cada livro tem 50 páginas.
* **Acesso por Coluna (Incorreto):** Você lê a página 1 do livro 1, depois corre para a estante, pega o livro 2 e lê a página 1, depois o livro 3... Quando termina a página 1 de todos os livros, volta ao livro 1 para ler a página 2. Você passou o dia inteiro trocando de livro!
* **Acesso por Linha (Correcto / Loop Interchange):** Você pega o livro 1 e lê as páginas de 1 a 50 sequencialmente. Depois pega o livro 2 e lê de 1 a 50. O esforço físico foi drasticamente reduzido!

### Diagrama Ilutrativo: Ordem de Varredura

```
[ Layout de Memória em C (Row-Major) ]
Endereço:  0x00  0x04  0x08  0x0C | 0x10  0x14  0x18  0x1C
Dado:     A[0][0] A[0][1] A[0][2] A[0][3] | A[1][0] A[1][1] A[1][2] A[1][3]

Varredura sem Interchange (j externo, i interno - Pior Desempenho):
Passo 1: A[0][0] -> Passo 2: A[1][0] -> Passo 3: A[2][0] (Saltos gigantes na RAM!)

Varredura COM Interchange (i externo, j interno - Desempenho Máximo):
Passo 1: A[0][0] -> Passo 2: A[0][1] -> Passo 3: A[0][2] (Leitura contígua em Cache!)
```

![Diagrama indicando a diferença entre acesso por coluna (Saltos) vs. acesso por linha (Unit Stride)](/workspace/scratch/aula3_2/loop_interchange_diagram.png)
*(Sugestão de Imagem: Um diagrama mostrando uma matriz 2D com setas vermelhas saltando entre linhas vs. setas verdes deslizando continuamente ao longo das linhas)*

### Exemplo em C: Antes e Depois

```c
#define N 4000
double A[N][N];

// --- ANTES (Ineficiente: Saltos por coluna / Non-Unit Stride) ---
void varredura_ruim() {
    for (int j = 0; j < N; j++) {
        for (int i = 0; i < N; i++) {
            A[i][j] = A[i][j] * 2.0; // i muda no laço interno -> pula N*8 bytes por iteração!
        }
    }
}

// --- DEPOIS (Loop Interchange Aplicado: Unit Stride) ---
void varredura_otimizada() {
    for (int i = 0; i < N; i++) {       // Invertemos os laços 'i' e 'j'
        for (int j = 0; j < N; j++) {
            A[i][j] = A[i][j] * 2.0; // j muda no laço interno -> acesso 100% contíguo!
        }
    }
}
```

---

## 2. Loop Tiling / Blocking (Blocagem de Laços)

### O Problema da Reutilização de Dados em Matrizes Grandes
Suponha que você esteja multiplicando duas matrizes grandes ($N = 2000$). Para calcular um único elemento da matriz resultante, você precisa ler uma linha inteira da Matriz A e uma coluna inteira da Matriz B.

Se a matriz não couber inteira na **Cache L1 ou L2**, quando a CPU terminar de ler a coluna e precisar reusar os dados para o próximo cálculo, a cache já terá **descartado (*evicted*)** as linhas anteriores. O resultado é que os dados são lidos repetidamente da memória RAM lenta dezenas de milhares de vezes!

### O Conceito de Loop Tiling
O **Loop Tiling** (ou *Blocking*) divide uma matriz gigante em submatrizes menores chamadas **Blocos** ou **Tiles** (Ladrilhos) de tamanho $B 	imes B$. 

Escolhe-se o tamanho $B$ de forma que o bloco inteiro de dados caiba confortavelmente dentro da Cache L1 (ex: 32 KB) ou L2 (ex: 512 KB). A CPU processa **completamente** todas as operações daquele bloco antes de passar para o bloco seguinte!

### Analogia do Pintor de Parede
* **Sem Tiling:** O pintor passa uma pincelada fina de 20 metros de comprimento no topo da parede. Depois anda 20 metros de volta para passar a segunda pincelada um pouco abaixo. A tinta da primeira ponta já secou e ele perdeu o ritmo.
* **Com Tiling (Blocagem):** O pintor divide a parede em quadros de $1 	imes 1$ metro. Ele pinta completamente aquele quadro pequeno, garante o acabamento impecável, e só então move a escada para o bloco ao lado.

### Diagrama de Multiplicação por Blocos

```
Matriz Completa (N x N)              Dividida em Blocos (B x B)
+-----------------------+            +---+---+---+---+
|                       |            |B00|B01|B02|B03|
|                       |  ======>   +---+---+---+---+
|       N x N           |            |B10|B11|B12|B13|  <-- Cada Bloco BxB
|                       |            +---+---+---+---+      cabe inteiro na L1 Cache!
+-----------------------+            +---+---+---+---+
```

![Ilustração da técnica de Loop Tiling dividindo uma matriz em blocos BxB](/workspace/scratch/aula3_2/loop_tiling_diagram.png)
*(Sugestão de Imagem: Um grid representando uma grande matriz destacando em azul um pequeno bloco BxB sendo processado iterativamente)*

### Exemplo em C: Multiplicação de Matrizes (Ingênuo vs. Tiled)

```c
#define N 2000
#define B 32 // Tamanho do Bloco (32x32 doubles = 8 KB -> Cabe perfeitamente na L1)

double A[N][N], B_mat[N][N], C[N][N];

// --- 1. Algoritmo Tradicional O(N^3) (Estoura a Cache constantemente) ---
void matmul_classico() {
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            C[i][j] = 0.0;
            for (int k = 0; k < N; k++) {
                C[i][j] += A[i][k] * B_mat[k][j];
            }
        }
    }
}

// --- 2. Algoritmo com Loop Tiling (Aproveitamento Máximo da Cache) ---
void matmul_tiled() {
    // Laços externos navegam DE BLOCO EM BLOCO
    for (int ii = 0; ii < N; ii += B) {
        for (int jj = 0; jj < N; jj += B) {
            for (int kk = 0; kk < N; kk += B) {
                
                // Laços internos processam DENTRO DO BLOCO B x B
                for (int i = ii; i < ii + B; i++) {
                    for (int j = jj; j < jj + B; j++) {
                        double sum = C[i][j];
                        for (int k = kk; k < kk + B; k++) {
                            sum += A[i][k] * B_mat[k][j];
                        }
                        C[i][j] = sum;
                    }
                }

            }
        }
    }
}
```

---

## 3. Loop Unrolling (Desenrolamento de Laços)

### O Conceito
O **Loop Unrolling** consiste em duplicar o corpo de um laço para executar múltiplas iterações dentro de um único passo de controle.

Em um laço tradicional, a cada iteração a CPU precisa:
1. Executar a instrução útil do corpo.
2. Incrementar a variável de controle (`i++`).
3. Comparar a variável com o limite (`i < N`).
4. Executar um salto condicional (`branch`) de volta ao início.

Essas instruções de controle geram um **overhead (sobrecarga)** significativo se o corpo do laço for muito simples.

### Analogia das Compras no Mercado
* **Sem Unrolling:** Você vai ao mercado, compra 1 maçã, volta para casa. Vai ao mercado, compra 1 maçã, volta para casa... Você repetiu a viagem (overhead) 4 vezes para comprar 4 maçãs.
* **Com Unrolling (Fator 4):** Você vai ao mercado uma única vez e traz 4 maçãs de uma só vez. Você cortou 75% dos deslocamentos!

### Exemplo em C: Unrolling Manual

```c
#define N 1000000
double a[N], b[N], c[N];

// --- ANTES (Laço Tradicional - 1.000.000 de verificações de salto) ---
void soma_normal() {
    for (int i = 0; i < N; i++) {
        c[i] = a[i] + b[i];
    }
}

// --- DEPOIS (Loop Unrolling com Fator 4 - Apenas 250.000 verificações) ---
void soma_unrolled() {
    int i;
    // Processa de 4 em 4 elementos
    for (i = 0; i < N - 3; i += 4) {
        c[i]   = a[i]   + b[i];
        c[i+1] = a[i+1] + b[i+1];
        c[i+2] = a[i+2] + b[i+2];
        c[i+3] = a[i+3] + b[i+3];
    }
    
    // Laço de limpeza (Remainder Loop) para cobrir eventuais sobras caso N não seja múltiplo de 4
    for (; i < N; i++) {
        c[i] = a[i] + b[i];
    }
}
```

### Principais Benefícios do Loop Unrolling
1. **Redução de Saltos (*Branch Reduction*):** Diminui a quantidade de desvios e alivia o preditor de desvios (*Branch Predictor*) da CPU.
2. **Paralelismo em Nível de Instrução (ILP):** Permite que a CPU superescalar execute instruções independentes (`c[i]` e `c[i+1]`) simultaneamente nas suas múltiplas Unidades Lógicas e Aritméticas (ALUs).
3. **Facilita a Vetorização SIMD:** Abre espaço para o compilador empacotar os dados em registradores vetoriais (AVX/SSE).

---

## 4. Otimização Combinada pelo Compilador e Cuidado com Abusos

Os compiladores C/C++ modernos (GCC, Clang, ICC) realizam essas transformações automaticamente quando compilados com flags de otimização como `-O2`, `-O3` ou `-ffast-math`.

Você também pode guiar o compilador usando pragmas específicos:

```c
// Instruindo o compilador a desenrolar o laço automaticamente
#pragma omp simd
#pragma unroll(4)
for (int i = 0; i < N; i++) {
    a[i] = b[i] * factor;
}
```

### Cuidado: A Armadilha da Pressão nos Registradores (*Register Pressure*)
Desenrolar laços excessivamente (ex: fator 16 ou 32) pode piorar o desempenho!
* Se o corpo do laço exigir mais variáveis temporárias do que a quantidade de registradores físicos disponíveis na CPU, o compilador será forçado a salvar variáveis temporárias na pilha da RAM (**Spilling**).
* Isso reintroduz acessos lentos à memória, anulando todos os ganhos de desempenho.

---

## Tabela Comparativa das Transformações de Laço

| Técnica | Objetivo Principal | Principal Benefício | Risco / Cuidado |
| :--- | :--- | :--- | :--- |
| **Loop Interchange** | Ajustar a ordem do laço ao layout da RAM. | Garante acesso sequencial (*Unit Stride*). | Verificar dependências de dados entre iterações. |
| **Loop Tiling (Blocking)** | Dividir dados em blocos $B 	imes B$. | Maximiza o reuso de dados na Cache L1/L2. | Escolher o tamanho do bloco $B$ adequado à cache. |
| **Loop Unrolling** | Replicar o corpo do laço $K$ vezes. | Reduz overhead de saltos e estimula ILP/SIMD. | *Register Pressure* e aumento do tamanho do binário. |

---

## Exercícios de Fixação

1. **Análise de Código:** Considere o laço abaixo em C. Por que ele apresenta péssimo desempenho e qual transformação de laço resolveria o problema?
   ```c
   for (int j = 0; j < 5000; j++)
       for (int i = 0; i < 5000; i++)
           matriz[i][j] += 1.0;
   ```
2. **Cálculo de Bloco (Tiling):** Se a Cache L1 de um processador possui 32 KB dedicados a dados e cada elemento da sua matriz é um `double` (8 bytes), qual é o maior valor do bloco $B$ (para um bloco quadrado $B 	imes B$) de forma que **duas** submatrizes desse tamanho caibam simultaneamente na L1?
3. **Explicação Conceitual:** Por que o **Loop Unrolling** exige um "laço de limpeza" (*remainder loop*) no final da função?

---
*Próxima Aula: Aula 3.3 — Diagnóstico e Mitigação de False Sharing em Código Paralelo.*
