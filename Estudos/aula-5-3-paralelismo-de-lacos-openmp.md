# Aula 5.3: Paralelismo de Laços no OpenMP — Constructo `for`, Agendamentos (`schedule`), `reduction` e `nowait`

Welcome à **Aula 5.3**! Se na Aula 5.1 aprendemos a disparar um time de *threads* com o `#pragma omp parallel` e na Aula 5.2 descobrimos como controlar a visibilidade e o escopo de dados (`shared`, `private`, `reduction`), agora chegou a hora de aprender a ferramenta mais utilizada e poderosa do OpenMP: **o paralelismo de laços de repetição (`for`)**.

Em grande parte das aplicações científicas e de alto desempenho, até 90% do tempo de execução é gasto processando grandes matrizes e vetores dentro de laços. O OpenMP nos permite fatiar e distribuir as iterações desses laços entre múltiplos núcleos de processamento com pouquíssimas linhas de código.

nesta aula, vamos dominar o constructo `#pragma omp for`, compreender em detalhes os diferentes modos de agendamento (`static`, `dynamic`, `guided`), entender a mágica por trás da cláusula `reduction` e aprender a eliminar esperas desnecessárias com a cláusula `nowait`.

---

## 1. O Constructo `#pragma omp for` e a Forma Canônica

### 1.1 Do `#pragma omp parallel` ao `#pragma omp for`

Um erro muito comum entre iniciantes é acreditar que colocar apenas `#pragma omp parallel` sobre um laço `for` dividirá o trabalho entre as *threads*.

* **Sem o `for`:** Se você criar uma região paralela com 4 *threads* e colocar um `for (int i = 0; i < 100; i++)` lá dentro, **todas as 4 threads executarão o laço completo de 0 a 99**. O trabalho será duplicado 4 vezes!
* **Com o `for`:** A diretiva `#pragma omp for` avisa ao OpenMP: *"Não duplique este laço! Pegue as 100 iterações e divida-as entre o time de threads existente."*

Por conveniência, o OpenMP permite combinar as duas diretivas em um único pragma fundido: **`#pragma omp parallel for`**.

```c
// Forma combinada mais utilizada no dia a dia:
#pragma omp parallel for
for (int i = 0; i < N; i++) {
    processa_elemento(i);
}
```

---

### 1.2 A Analogia da Fila do Restaurante / Linha de Produção

Imagine uma cozinha industrial com **100 pratos** para montar e **4 cozinheiros** (*threads*):
* **Com `#pragma omp parallel` apenas:** Todos os 4 cozinheiros montam os mesmos 100 pratos do início ao fim (desperdício total e replicação de trabalho).
* **Com `#pragma omp parallel for`:** O gerente divide a ficha dos 100 pratos em 4 lotes de 25 pratos. Cada cozinheiro monta apenas a sua fatia atribuída, finalizando os 100 pratos em 1/4 do tempo!

```
                    [ 100 Iterações do Laço FOR ]
                                  │
         ┌────────────────────────┼────────────────────────┐
         ▼                        ▼                        ▼
┌─────────────────┐      ┌─────────────────┐      ┌─────────────────┐
│ Thread 0 (0..24)│      │ Thread 1 (25..49│      │ Thread 2 (50..74│ ...
└─────────────────┘      └─────────────────┘      └─────────────────┘
```

---

### 1.3 Requisitos Obrigatórios: A Forma Canônica do Laço

Nem todo laço `for` em C pode ser paralelizado automaticamente pelo OpenMP. Para que o runtime consiga calcular antecipadamente a quantidade exata de iterações (*Countable Loop*), o laço precisa seguir a **Forma Canônica**:

1. **Formato do Controle de Laço:** Deve ser do tipo `for (var = inicio; var op limite; var incr)`.
   * Exemplos válidos: `i = 0; i < N; i++`, `i = N; i > 0; i--`, `i = 0; i < N; i += 2`.
2. **Índice de Controle do Tipo Inteiro ou Ponteiro:** Variáveis de controle `float` ou `double` não são permitidas na forma canônica.
3. **Limites Invariantes:** As variáveis `inicio` e `limite` não podem ser alteradas dentro do corpo do laço.
4. **Proibição de Saltos de Saída Antecipada:** O laço não pode conter chamadas a `break`, `return`, `goto` ou exceções que forcem a saída antes do término natural das iterações.

---

## 2. Estratégias de Agendamento (`schedule`)

Como o OpenMP deve fatiarem e entregar as iterações para as *threads*? Dependendo da natureza do problema, algumas iterações podem demorar milissegundos enquanto outras demoram segundos. Escolher o modo correto de **agendamento (*scheduling*)** é a chave para evitar desbalanceamento de carga (*load imbalance*).

A sintaxe geral é: `#pragma omp parallel for schedule(tipo, chunk_size)`

```
┌────────────────────────────────────────────────────────────────────────┐
│                        Estratégias de Schedule                         │
├──────────────┬──────────────────────────────────┬──────────────────────┤
│ Tipo         │ Mecanismo de Distribuição        │ Overhead de Sinc.    │
├──────────────┼──────────────────────────────────┼──────────────────────┤
│ static       │ Divisão fixa prévia (compilação) │ Mínimo (Zero)        │
│ dynamic      │ Fila sob demanda no runtime      │ Médio / Alto         │
│ guided       │ Chunks dinâmicos decrescentes    │ Baixo / Médio        │
│ runtime/auto │ Definido via OMP_SCHEDULE ou SO  │ Variável             │
└──────────────┴──────────────────────────────────┴──────────────────────┘
```

---

### 2.1 `schedule(static, chunk_size)`

* **Como Funciona:** As iterações são divididas em blocos fixos de tamanho `chunk_size` e atribuídas às *threads* de forma cíclica do tipo *Round-Robin* **antes do laço começar**. Se `chunk_size` for omitido, o OpenMP divide o total de iterações em $P$ blocos iguais de tamanho aproximadamente $N/P$.
* **Vantagens:** Determinístico e com **overhead praticamente nulo** (sem travas ou comunicação durante o laço). Excelente para localidade de cache L1/L2.
* **Quando Usar:** Quando o trabalho de cada iteração for homogêneo e previsível (ex: soma de vetores, multiplicação de matrizes densas).
* **Analogia:** Um professor distribuindo 100 provas divididas igualmente em 4 pilhas fixas para 4 monitores antes do início do plantão.

---

### 2.2 `schedule(dynamic, chunk_size)`

* **Como Funciona:** As iterações são divididas em blocos de tamanho `chunk_size`. Cada *thread* pega um bloco de uma fila central. Assim que a *thread* termina seu bloco, ela retorna à fila e solicita um novo bloco até que todas as iterações sejam concluídas. Se omitido, o `chunk_size` padrão é 1.
* **Vantagens:** Perfeita para **cargas heterogêneas e altamente desbalanceadas**, onde o tempo de execução de cada iteração varia drasticamente.
* **Desvantagens:** Alto *overhead* de sincronização (exige mecanismos atômicos/travas na fila central a cada bloco distribuído).
* **Quando Usar:** Conjunto de Mandelbrot, ray tracing, problemas onde a carga computacional depende dos dados do elemento.
* **Analogia:** Senhas de atendimento em um banco. O cliente que pega a senha vai ao próximo caixa livre. Se um atendimento demorar, outros caixas continuam atendendo a fila.

---

### 2.3 `schedule(guided, chunk_size)`

* **Como Funciona:** Similar ao `dynamic` (distribuição sob demanda), porém o tamanho dos blocos **diminui exponencialmente** com o tempo. O tamanho do bloco inicial é proporcional a $rac{	ext{Iterações Restantes}}{	ext{Número de Threads}}$, reduzindo gradualmente até atingir o limite mínimo definido por `chunk_size` (padrão é 1).
* **Vantagens:** Combina o baixo *overhead* do `static` no início do laço (blocos grandes) com a capacidade de balanceamento de carga do `dynamic` no final (blocos pequenos para ajustar discrepâncias).
* **Quando Usar:** Laços desbalanceados e com grande número de iterações, servindo como meio-termo ideal entre `static` e `dynamic`.

---

### 2.4 `schedule(runtime)` e `schedule(auto)`

* **`runtime`:** O modo de agendamento não é fixado no código-fonte em C. Ele é lido em tempo de execução a partir da variável de ambiente `OMP_SCHEDULE` (ex: `export OMP_SCHEDULE="dynamic,8"`). Isso permite testar diferentes estratégias de desempenho sem recompilar o programa!
* **`auto`:** Delega a decisão do agendamento totalmente ao compilador e ao runtime do OpenMP, que podem utilizar métricas históricas de otimização.

---

## 3. A Cláusula `reduction` em Laços

Quando acumulamos valores dentro de um laço (ex: somatório $\sum$, produtório $\prod$, mínimos ou máximos), temos uma **condição de corrida (*race condition*)** se múltiplos núcleos tentarem atualizar a mesma variável `shared` ao mesmo tempo.

A cláusula **`reduction(operador : variavel)`** resolve esse problema de forma automática, transparente e ultraeficiente:

1. O OpenMP cria uma cópia privada da variável para cada *thread*, inicializada com o elemento neutro do operador (ex: `0` para soma, `1` para multiplicação, $+\infty$ para mínimo).
2. Cada *thread* acumula livremente em sua variável privada dentro do laço paralelo sem qualquer travamento de memória.
3. Ao final do laço (no ponto de *Join*), o OpenMP combina todas as cópias privadas com a variável original usando operações atômicas ou em árvore binária.

```c
double soma = 0.0;

#pragma omp parallel for reduction(+:soma)
for (int i = 0; i < N; i++) {
    soma += array[i];
}
```

---

## 4. A Cláusula `nowait`: Eliminando Barreiras Implícitas

Por padrão, ao final de qualquer diretiva `#pragma omp for` (ou `#pragma omp single`, `#pragma omp sections`), existe uma **barreira de sincronização implícita (*implicit barrier*)**. Todas as *threads* mais rápidas são obrigadas a congelar e esperar a *thread* mais lenta terminar sua fatia de iterações antes de prosseguir para as linhas de código seguintes.

Se você possui dois laços consecutivos e **o segundo laço não depende dos resultados calculados pelo primeiro**, essa espera é um desperdício de ciclos de CPU!

A cláusula **`nowait`** instrui o runtime do OpenMP a remover a barreira de sincronização final. Assim que uma *thread* conclui suas iterações do primeiro laço, ela avança imediatamente para o laço seguinte sem esperar pelas demais.

```c
#pragma omp parallel
{
    // Primeiro laço sem barreira final:
    #pragma omp for nowait
    for (int i = 0; i < N; i++) {
        A[i] = B[i] + 1.0;
    }

    // Threads rápidas já começam o segundo laço imediatamente!
    #pragma omp for
    for (int i = 0; i < N; i++) {
        C[i] = D[i] * 2.0;
    } // Barreira implícita final da região paralela aqui
}
```

---

## 5. Exemplo Prático Completo em C

Abaixo temos um programa em C completo demonstrando o paralelismo de laço com `schedule(dynamic)`, cálculo de redução para estimativa de soma, e medição do tempo de execução com `omp_get_wtime()`.

```c
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <omp.h>

#define N 10000000

int main() {
    double *vetor = (double *)malloc(N * sizeof(double));
    if (vetor == NULL) {
        printf("Erro ao alocar memória!\n");
        return 1;
    }

    // Inicialização do vetor
    for (int i = 0; i < N; i++) {
        vetor[i] = (double)i * 0.0001;
    }

    double soma_total = 0.0;
    double inicio = omp_get_wtime();

    // Paralelização com schedule guided e redução
    #pragma omp parallel for schedule(guided, 1024) reduction(+:soma_total)
    for (int i = 0; i < N; i++) {
        // Carga de trabalho simulada com cálculo pesado
        soma_total += sin(vetor[i]) * cos(vetor[i]);
    }

    double fim = omp_get_wtime();

    printf("====================================================\n");
    printf("  PARALELISMO DE LAÇOS NO OPENMP (DESEMPENHO)      \n");
    printf("====================================================\n");
    printf("  Total de elementos (N): %d\n", N);
    printf("  Soma Calculada:        %.6f\n", soma_total);
    printf("  Tempo de Execução:     %.4f segundos\n", fim - inicio);
    printf("====================================================\n");

    free(vetor);
    return 0;
}
```

---

## 6. Tabela Comparativa das Estratégias de Agendamento

```
┌──────────────┬──────────────────────┬──────────────────────┬──────────────────────────┐
│ Agendamento  │ Distribuição de Carga│ Overhead Runtime     │ Caso de Uso Recomendado  │
├──────────────┼──────────────────────┼──────────────────────┼──────────────────────────┤
│ static       │ Pré-definida / Bloco │ Praticamente Zero    │ Cargas homogêneas e      │
│              │                      │                      │ matrizes contíguas.      │
├──────────────┼──────────────────────┼──────────────────────┼──────────────────────────┤
│ dynamic      │ Fila sob demanda     │ Alto (Sincronização) │ Cargas heterogêneas e    │
│              │                      │                      │ simulações irregulares.  │
├──────────────┼──────────────────────┼──────────────────────┼──────────────────────────┤
│ guided       │ Dinâmico decrescente │ Moderado             │ Grandes laços com        │
│              │                      │                      │ desbalanceamento leve.   │
├──────────────┼──────────────────────┼──────────────────────┼──────────────────────────┤
│ runtime      │ Definido via envvar  │ Variável             │ Testes e ajuste fino de  │
│              │ OMP_SCHEDULE         │                      │ desempenho sem recompilar│
└──────────────┴──────────────────────┴──────────────────────┴──────────────────────────┘
```

---

## 7. Exercícios de Fixação

1. **Por que colocar apenas `#pragma omp parallel` antes de um laço `for` não divide as iterações entre as threads?**
2. **Qual é a diferença fundamental entre os agendamentos `static` e `dynamic` no OpenMP? Em qual cenário o `dynamic` supera o `static`?**
3. **Explique a função da cláusula `nowait` e identifique em que situação ela NÃO deve ser utilizada.**
4. **Qual é o valor inicial da variável privada criada pela cláusula `reduction(*:produtorio)`? Justifique sua resposta com base no elemento neutro da operação.**

---

### Gabarito Comentado

1. **Resposta:** O `#pragma omp parallel` cria a região paralela e clona o bloco de código para todas as *threads*. Sem a diretiva de trabalho `#pragma omp for`, cada *thread* executará a totalidade das iterações do laço `for` de forma independente, resultando em trabalho duplicado e desnecessário.
2. **Resposta:** O agendamento `static` mapeia as iterações de forma fixa no início da execução com overhead quase nulo, enquanto o `dynamic` distribui blocos sob demanda via fila central. O `dynamic` supera o `static` quando as iterações possuem tempos de processamento muito heterogêneos (desbalanceamento de carga), pois evita que *threads* fiquem ociosas aguardando a mais lenta terminar.
3. **Resposta:** A cláusula `nowait` remove a barreira de sincronização implícita ao final de um bloco `#pragma omp for`, permitindo que *threads* rápidas avancem imediatamente para o código seguinte. Ela NÃO deve ser utilizada se as instruções ou laços seguintes dependerem dos dados calculados no laço atual.
4. **Resposta:** O valor inicial é `1.0` (ou `1` para inteiros). O OpenMP inicializa a variável privada de redução com o elemento neutro da operação especificada; para a multiplicação (`*`), o elemento neutro é $1$, pois qualquer número multiplicado por $1$ mantém seu valor original.
