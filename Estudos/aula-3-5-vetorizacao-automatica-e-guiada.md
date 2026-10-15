# Aula 3.5: Vetorização Automática e Guiada

## Introdução: O Compilador como seu Aliado SIMD

Na **Aula 3.4**, vimos como utilizar *intrinsics* do AVX2 (`<immintrin.h>`) para programar registradores vetoriais manualmente. Embora a vetorização manual ofereça controle total sobre o hardware, ela possui três grandes desvantagens:
1. **Baixa Produtividade:** Reescrever algoritmos complexos em assembly ou intrinsics exige muito tempo e esforço.
2. **Falta de Portabilidade:** Código escrito para AVX2 não roda em processadores sem suporte a AVX2 ou em arquiteturas ARM (que usam NEON ou SVE).
3. **Manutenibilidade Complexa:** Código repleto de funções como `_mm256_fmadd_ps` é difícil de ler e manter por outros desenvolvedores.

A **Vetorização Automática (*Auto-Vectorization*)** é a capacidade dos compiladores modernos (GCC, Clang, ICX) de analisar laços sequenciais comuns escritos em C/C++ e transformá-los automaticamente em instruções vetoriais SIMD. 

Quando o compilador não consegue vetorizar sozinho por incerteza quanto à segurança dos dados, podemos aplicar a **Vetorização Guiada**, fornecendo "dicas" estruturadas ao compilador por meio de pragmas e palavras-chave.

---

## 1. Níveis de Otimização e Flags de Compilação

Os compiladores necessitam de autorização explícita para aplicar otimizações agressivas que alterem a ordem ou a forma de execução das instruções.

### Principais Flags do GCC / Clang

*   **`-O0` (Sem otimização):** Código compilado exatamente como escrito. Ideal para depuração com `gdb`.
*   **`-O1` (Otimização básica):** Aplica redução de código e otimizações simples sem aumentar o tempo de compilação.
*   **`-O2` (Otimização padrão de produção):** Ativa quase todas as otimizações seguras que não envolvem trocas drásticas de espaço por velocidade.
*   **`-O3` (Otimização agressiva / Vetorização ativada):** Ativa a **vetorização automática de laços** por padrão no GCC e Clang, além de *loop unrolling* e inlining de funções.
*   **`-march=native`:** Instrui o compilador a gerar código otimizado especificamente para a arquitetura do processador onde a compilação está ocorrendo (liberando AVX2, AVX-512, FMA, etc.).
*   **`-ffast-math`:** Permite otimizações de ponto flutuante que ignoram o padrão estrito IEEE 754 (como reordenar somas para permitir reduções SIMD).

---

### Diagnóstico de Vetorização (*Compiler Diagnostics*)

Como saber se o compilador realmente vetorizou seu laço? Não adivinhe — pergunte ao compilador!

*   **No GCC:**
    ```bash
    gcc -O3 -march=native -fopt-info-vec-optimized -fopt-info-vec-missed programa.c -o programa
    ```
    *   `-fopt-info-vec-optimized`: Mostra quais laços **foram vetorizados com sucesso**.
    *   `-fopt-info-vec-missed`: Explica **por que** um laço específico **não puder ser vetorizado**.

*   **No Clang:**
    ```bash
    clang -O3 -march=native -Rpass=loop-vectorize -Rpass-missed=loop-vectorize programa.c -o programa
    ```

---

## 2. Requisitos Obrigatórios para um Laço Ser Vetorizável

Nem todo laço `for` pode ser vetorizado. Para que o compilador transforme a execução escalar em SIMD de forma segura, o laço deve atender a **quatro requisitos fundamentais**:

```
[ILUSTRAÇÃO / DIAGRAMA: Requisitos de Laço Vetorizável]
-----------------------------------------------------------------------
1. Contagem Conhecida   --> Quantidade de iterações calculável antes de entrar
2. Única Entrada/Saída  --> Sem break, return, goto, ou exceções no meio
3. Sem Chamadas Ocapas  --> Funções internas devem ser inline ou SIMD
4. Sem Dependência RAW  --> Iteração [i] não depende da escrita de [i-1]
-----------------------------------------------------------------------
```

### Requisito 1: Contagem de Iterações Conhecida (*Countable Loop*)
O número de iterações deve ser determinável antes do início da primeira iteração. Laços do tipo `while(condicao)` com parada indeterminada não são vetorizáveis.

*   ❌ **Incorreto:**
    ```c
    while (array[i] != 0) { // Contagem desconhecida previamente
        processa(array[i]);
        i++;
    }
    ```
*   ✓ **Correto:**
    ```c
    for (int i = 0; i < n; i++) { // 'n' é conhecido antes de iniciar
        array[i] *= 2.0f;
    }
    ```

### Requisito 2: Fluxo de Controle Simples (Única Entrada e Saída)
O laço não pode conter desvios de controle que interrompam abruptamente sua execução (`break`, `return`, `exit` ou `goto`).

*   ❌ **Incorreto:**
    ```c
    for (int i = 0; i < n; i++) {
        if (a[i] < 0) break; // Saída antecipada quebra a vetorização
        b[i] = sqrt(a[i]);
    }
    ```

### Requisito 3: Sem Chamadas a Funções Não-Inlined
Se o corpo do laço chamar uma função externa genérica, o compilador precisará interromper o fluxo vetorial para realizar um salto de função (*call*).

*   ❌ **Incorreto:** `b[i] = minha_funcao_externa(a[i]);`
*   ✓ **Correto:** Tornar a função `inline` ou usar funções matemáticas nativas vetorizáveis (como `sinf`, `cosf`, `sqrtf` com `-ffast-math`).

---

## 3. Análise de Dependência de Dados e *Aliasing* de Ponteiros

O maior obstáculo para a vetorização automática é a **dependência de dados entre iterações** e a incerteza sobre a sobreposição de memória (*Aliasing*).

### A. Dependência de Dados Cruzada (*Loop-Carried Dependency*)

Se a iteração $i$ precisa ler um valor que foi alterado na iteração $i-1$, o laço **não pode** ser executado em paralelo via SIMD.

*   ❌ **Dependência Verdadeira / RAW (Read-After-Write):**
    ```c
    for (int i = 1; i < n; i++) {
        a[i] = a[i - 1] + b[i]; // A iteração 'i' depende do resultado gravado na iteração 'i-1'
    }
    ```
    *Explicação:* Se empacotarmos 8 elementos em um registrador AVX, tentaremos calcular `a[1]`, `a[2]`, ..., `a[8]` simultaneamente. Porém, `a[2]` precisa do resultado final de `a[1]`, que ainda não foi gravado!

*   ✓ **Sem Dependência Inter-iterações:**
    ```c
    for (int i = 0; i < n; i++) {
        a[i] = b[i] + c[i]; // Todas as iterações são completamente independentes
    }
    ```

---

### B. O Problema do *Aliasing* de Ponteiros

Em C e C++, dois ponteiros distintos podem apontar para regiões de memória que se sobrepõem. O compilador precisa assumir o pior cenário por segurança (*conservadorismo do compilador*).

Considere a função:
```c
void soma_vetores(float *a, float *b, float *c, int n) {
    for (int i = 0; i < n; i++) {
        a[i] = b[i] + c[i];
    }
}
```

Se o ponteiro `a` apontar para a posição `b + 1`, alterar `a[0]` mudará o valor de `b[1]`. Como o compilador não sabe onde `a`, `b` e `c` estão na memória RAM, ele desativa a vetorização automática ou gera um código de checagem em tempo de execução extremamente pesado.

```
[ILUSTRAÇÃO / DIAGRAMA: Aliasing de Ponteiros]
Memória RAM:
[  b[0]  |  b[1]  |  b[2]  |  b[3]  ]
           ^
           +--- Ponteiro 'a' aponta para cá! (a[0] altera b[1])
```

---

## 4. Vetorização Guiada: `restrict` e Pragmas

Para "destravar" o compilador e garantir a vetorização, utilizamos recursos da linguagem e pragmas do compilador.

### 1. A Palavra-Chave `restrict` (C99 / C11)

A palavra-chave `restrict` aplicada a um ponteiro promete ao compilador que aquela região de memória **não é acessada por nenhum outro ponteiro** dentro daquele escopo.

```c
void soma_vetores_otimizada(float * restrict a, 
                           const float * restrict b, 
                           const float * restrict c, 
                           int n) {
    for (int i = 0; i < n; i++) {
        a[i] = b[i] + c[i]; // Agora o compilador sabe que não há aliasing e vetoriza!
    }
}
```

---

### 2. Pragmas de Vetorização

Quando você sabe que um laço é seguro, mas o compilador recusa a vetorização por cautela, é possível forçá-la usando pragmas:

#### A. `#pragma omp simd` (Padrão OpenMP 4.0+)
É o método **mais portátil** em C, C++ e Fortran. Funciona no GCC, Clang, Intel ICX e MSVC.

```c
#include <omp.h>

void processa(float *a, float *b, int n) {
    #pragma omp simd
    for (int i = 0; i < n; i++) {
        a[i] = sqrtf(b[i]) + 1.0f;
    }
}
```

#### B. Pragmas Específicos do Compilador
*   **GCC:** `#pragma GCC ivdep` (*Ignore Vector Dependencies*)
*   **Clang:** `#pragma clang loop vectorize(enable)`

---

## 5. Exemplo Prático Completo em C

Abaixo está uma demonstração prática comparando um laço não vetorizado (por conta de aliasing e falta de dicas) contra o mesmo laço otimizado para vetorização automática.

### Código (`vetorizacao_guiada.c`)

```c
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define N 10000000 // 10 milhões de elementos

// 1. Versão Sem Dicas (Compilador fica conservador se não usar -O3)
void multiplicacao_lenta(float *a, float *b, float *c, float escalar, int n) {
    for (int i = 0; i < n; i++) {
        a[i] = b[i] * c[i] + escalar;
    }
}

// 2. Versão Guiada (Uso de restrict e #pragma omp simd)
void multiplicacao_vetorizada(float * restrict a, 
                              const float * restrict b, 
                              const float * restrict c, 
                              float escalar, int n) {
    #pragma omp simd
    for (int i = 0; i < n; i++) {
        a[i] = b[i] * c[i] + escalar;
    }
}

int main() {
    float *a = aligned_alloc(32, N * sizeof(float));
    float *b = aligned_alloc(32, N * sizeof(float));
    float *c = aligned_alloc(32, N * sizeof(float));

    for (int i = 0; i < N; i++) {
        b[i] = i * 0.5f;
        c[i] = i * 2.0f;
    }

    struct timespec inicio, fim;

    // Teste 1: Versão Convencional
    clock_gettime(CLOCK_MONOTONIC, &inicio);
    multiplicacao_lenta(a, b, c, 3.14f, N);
    clock_gettime(CLOCK_MONOTONIC, &fim);
    double tempo_lento = (fim.tv_sec - inicio.tv_sec) + (fim.tv_nsec - inicio.tv_nsec) / 1e9;

    // Teste 2: Versão Vetorizada
    clock_gettime(CLOCK_MONOTONIC, &inicio);
    multiplicacao_vetorizada(a, b, c, 3.14f, N);
    clock_gettime(CLOCK_MONOTONIC, &fim);
    double tempo_vetorizado = (fim.tv_sec - inicio.tv_sec) + (fim.tv_nsec - inicio.tv_nsec) / 1e9;

    printf("Tempo sem dicas:       %.5f segundos\n", tempo_lento);
    printf("Tempo guiado (SIMD):   %.5f segundos\n", tempo_vetorizado);
    printf("Speedup obtido:        %.2fx\n", tempo_lento / tempo_vetorizado);

    free(a); free(b); free(c);
    return 0;
}
```

### Compilando e Analisando os Relatórios

Compilando com o GCC ativando os relatórios de vetorização:

```bash
gcc -O3 -march=native -fopenmp-simd -fopt-info-vec-optimized vetorizacao_guiada.c -o vetorizacao_guiada
```

**Saída típica do compilador:**
```text
vetorizacao_guiada.c:20:5: note: loop vectorized using 32 byte vectors (AVX2)
vetorizacao_guiada.c:11:5: note: loop vectorized using 32 byte vectors (AVX2)
```

**Saída de execução típica:**
```text
Tempo sem dicas:       0.01820 segundos
Tempo guiado (SIMD):   0.00310 segundos
Speedup obtido:        5.87x
```

---

## Resumo do Módulo 3: Otimização Sequencial e Vetorização

Chegamos ao fim do **Módulo 3**! Vamos consolidar os aprendizados fundamentais que conectam a memória e o processamento escalar ao SIMD:

1. **Aula 3.1 (Acesso à Memória):** Dê preferência absoluta a acessos contíguos (*Unit Stride* / linha por linha) para maximizar o reuso de linhas de cache e prefetchers.
2. **Aula 3.2 (Reestruturação de Laços):** Use *Loop Interchange* para alinhar laços ao layout em memória e *Loop Tiling* para caber dados grandes nas caches L1/L2.
3. **Aula 3.3 (False Sharing):** Evite que *threads* distintas modifiquem variáveis vizinhas na mesma linha de cache de 64 bytes usando variáveis locais na pilha ou *padding*.
4. **Aula 3.4 (SIMD Manual):** Use registradores estendidos (`XMM`, `YMM`, `ZMM`) para executar a mesma operação sobre vetores de dados em um único ciclo.
5. **Aula 3.5 (Vetorização Automática/Guiada):** Escreva laços limpos, sem dependências RAW, marque ponteiros com `restrict` e use `-O3 -march=native` e `#pragma omp simd`.

---

## Exercícios de Fixação

1. **Por que o compilador recusa vetorizar o laço abaixo?**
   ```c
   for (int i = 0; i < n; i++) {
       if (valida(a[i])) {
           soma += a[i];
       } else {
           break;
       }
   }
   ```
   *a)* Por falta de memória RAM.  
   *b)* Por conter uma instrução `break` que torna a contagem de iterações indeterminada previamente.  
   *c)* Porque somas não podem ser vetorizadas.  
   *d)* Porque a variável `soma` está na pilha.  

2. **Qual é a função da palavra-chave `restrict` em C99 no contexto de vetorização?**  
   *a)* Impedir que a função seja chamada por mais de uma thread simultaneamente.  
   *b)* Garantir ao compilador que os ponteiros não se sobrepõem na memória (*aliasing*), liberando a vetorização SIMD.  
   *c)* Restringir o tamanho do vetor para no máximo 64 elementos.  
   *d)* Forçar o armazenamento da variável em um registrador da CPU.

3. **Qual conjunto de flags do GCC ativa a vetorização e gera instruções específicas para o processador local?**  
   *a)* `-O0 -g`  
   *b)* `-O2 -Wall`  
   *c)* `-O3 -march=native`  
   *d)* `-Os -c`  

---

### Gabarito
1. **b** — A presença do `break` impede que o compilador determine o número fixo de iterações antes de entrar no laço.
2. **b** — `restrict` elimina a incerteza do compilador sobre a sobreposição de ponteiros (*aliasing*).
3. **c** — `-O3` ativa a vetorização automática e `-march=native` libera o uso das extensões AVX2/AVX-512 presentes na CPU.
