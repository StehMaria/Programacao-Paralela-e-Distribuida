# Aula 5.4: Paralelismo de Tarefas no OpenMP — Constructos `task`, `taskwait`, `single` e Dependências (`depend`)

---

## 1. Introdução: Paralelismo de Dados vs. Paralelismo de Tarefas

Nas aulas anteriores (especialmente na **Aula 5.3**), exploramos o **Paralelismo de Dados** (*Data Parallelism*). Ele é perfeito quando temos um grande volume de dados organizados de forma homogênea (como vetores e matrizes) e aplicamos exatamente a mesma operação em cada elemento através de laços `for`.

No entanto, o mundo real dos algoritmos nem sempre é uma matriz certinha. Muitas vezes nos deparamos com:
1. **Estruturas de Dados Irregulares:** Árvores, grafos, listas encadeadas.
2. **Algoritmos Recursivos:** Divisão e Conquista (como *Quicksort*, *Mergesort*, navegação em árvores).
3. **Funções Heterogêneas:** Tarefas com tempos de execução completamente imprevisíveis ou etapas distintas executadas em paralelo.

Tentá-los paralelizar apenas com `#pragma omp for` é como tentar encaixar um pino redondo em um buraco quadrado. Para esses cenários, o OpenMP introduziu o **Paralelismo de Tarefas (*Task Parallelism*)**.

> 💡 **Analogia da Cozinha Profissional:**
> * **Paralelismo de Dados (`#pragma omp for`):** 4 cozinheiros cortando 100 tomates. O trabalho é perfeitamente fatiado em blocos iguais (25 tomates para cada um).
> * **Paralelismo de Tarefas (`#pragma omp task`):** Um chef gerencia comandas variadas que chegam. Uma comanda pede "assal um peixe" (demora 30 min), outra pede "preparar molho" (5 min) e outra pede "picar cebola" (2 min). O chef empacota cada comanda em uma **Tarefa** e a joga no mural. Os cozinheiros disponíveis pegam tarefas do mural à medida que ficam livres!

---

## 2. A Mecânica do Modelo de Tarefas no OpenMP

### O Conceito de Tarefa (*Task*)
No OpenMP, uma **Tarefa** é uma unidade autônoma de trabalho composta por:
* Um bloco de código a ser executado.
* Seu contexto de dados (variáveis associadas no momento da criação).

### A Fila de Tarefas (*Task Queue*) e *Work Stealing*
Quando o compilador encontra uma diretiva `#pragma omp task`, a thread corrente **não executa a tarefa imediatamente**. Em vez disso, ela empacota a tarefa e a coloca em uma **Fila de Tarefas (*Task Queue*)**.

As threads do time (*thread pool*) atuam como trabalhadoras ativas:
1. Se uma thread estiver ociosa, ela retira uma tarefa da fila e a executa.
2. Se a fila de uma thread estiver vazia, ela usa a estratégia de **Roubo de Trabalho (*Work Stealing*)**, buscando tarefas na fila de outras threads que estejam sobrecarregadas.

```
+-----------------------------------------------------------------------+
|              ILUSTRAÇÃO: FILA DE TAREFAS E WORK STEALING               |
+-----------------------------------------------------------------------+
|                                                                       |
|   [ Master Thread ] ---> Gera Tarefas ---> [ Fila Global / Local ]    |
|                                                     |                 |
|                   +---------------------------------+                 |
|                   |                |                |                 |
|                   v                v                v                 |
|             [ Thread 1 ]     [ Thread 2 ]     [ Thread 3 ]            |
|              (Executando)     (Executando)    (Ociosa -> Rouba)       |
|                                                                       |
+-----------------------------------------------------------------------+
```
*(Indicação de Imagem: Caso haja um arquivo visual de tarefas/OpenMP, substitua pelo diagrama correspondente ao Task Queue do OpenMP).*

---

## 3. O Padrão Gerador: `#pragma omp single` e `#pragma omp master`

Para evitar que *todas* as threads criem cópias duplicadas das mesmas tarefas, é um padrão universal no OpenMP usar uma **única thread** para percorrer a estrutura de dados e "semear" as tarefas.

### O Constructo `#pragma omp single`
Garante que o bloco de código estruturado anexado será executado por **apenas uma thread** do time (não necessariamente a Thread 0). As outras threads saltam o bloco e esperam na barreira implícita ao final do `single` (a menos que a cláusula `nowait` seja adicionada).

```c
#pragma omp parallel
{
    #pragma omp single
    {
        // Apenas UMA thread entra aqui e gera as tarefas para o time todo!
        #pragma omp task
        fazer_tarefa_A();

        #pragma omp task
        fazer_tarefa_B();
    } // Barreira implícita: todas as threads esperam a criação terminar antes de prosseguir
}
```

### `#pragma omp single` vs. `#pragma omp master` / `masked`
* `#pragma omp single`: Executado pela primeira thread que chegar lá. Possui **barreira de sincronização implícita** ao final.
* `#pragma omp master` (ou `masked` no OpenMP 5.1+): Executado estritamente pela **Thread 0**. **NÃO possui barreira implícita** ao final.

---

## 4. O Escopo de Dados em Tarefas: A Regra do `firstprivate`

O comportamento das variáveis dentro de `#pragma omp task` tem uma particularidade **CRUCIAL** que difere dos laços `for`:

> ⚠️ **Atenção:** Como as tarefas são executadas de forma assíncrona no futuro por qualquer thread, variáveis locais do escopo externo (como ponteiros, contadores de laço `i`, etc.) podem ser modificadas ou destruídas pela thread geradora antes que a tarefa seja executada!
>
> Por essa razão, por padrão, variáveis privadas declaradas fora da tarefa são tratadas como **`firstprivate`** dentro da tarefa!

Isso significa que o OpenMP tira uma **"fotografia" (cópia)** do valor da variável no exato instante em que a tarefa é **criada** e guarda essa cópia junto do pacote da tarefa.

```c
int i = 10;
#pragma omp task // 'i' é implicitamente firstprivate!
{
    // Usa a cópia do valor 10, mesmo se a thread principal alterar 'i' fora daqui.
    printf("Valor de i na tarefa: %d
", i);
}
i = 20; // Alterar 'i' aqui não corrompe a tarefa já enfileirada.
```

---

## 5. Sincronização de Tarefas: `taskwait` e `taskgroup`

Como as tarefas são executadas de forma assíncrona, precisamos de mecanismos para dizer: *"Espere as tarefas terminarem antes de usar o resultado!"*.

### 1. `#pragma omp taskwait`
Pausa a execução da thread corrente até que **todas as tarefas filhas diretas** geradas por ela até aquele ponto tenham concluído sua execução.

> 🏢 **Analogia do Gerente:** O gerente delega 3 relatórios para 3 subordinados. Ele executa `#pragma omp taskwait` e senta na mesa até que os 3 relatórios estejam prontos sobre sua mesa. Só então ele junta os relatórios e escreve a conclusão.

```c
#pragma omp task
processar_parte_1();

#pragma omp task
processar_parte_2();

#pragma omp taskwait // Bloqueia até que a parte 1 e parte 2 terminem!

combinar_resultados(); // Seguro! Os dados já foram calculados.
```

### 2. `#pragma omp taskgroup`
O `taskwait` espera apenas pelas filhas **diretas**. Se uma tarefa filha criar tarefas "netas", o `taskwait` básico não espera pelas netas!
O `#pragma omp taskgroup` cria um bloco englobante e garante que **todas as tarefas descendentes** (filhas, netas, bisnetas) criadas dentro daquele bloco sejam concluídas antes de sair.

---

## 6. O Limite de Corte (*Cutoff Threshold*) em Algoritmos Recursivos

Ao paralelizar algoritmos recursivos (como Fibonacci ou Quicksort), criar uma tarefa OpenMP para cada nó da árvore de recursão gera um problema grave: **Overhead de Gerenciamento**.

Se o trabalho de uma tarefa for menor que o custo que o runtime do OpenMP leva para alocar a tarefa na memória e gerenciar a fila (ex: calcular `fib(2)`), o programa paralelo ficará **muito mais lento** que o sequencial!

### A Solução: Limite de Corte (*Cutoff*)
Abaixo de um determinado tamanho de problema, paramos de criar tarefas paralelas e chamamos a função **sequencial pura**.

```c
long fib_paralelo(int n) {
    if (n < 2) return n;

    // LIMITE DE CORTE (CUTOFF): Se n for pequeno, executa sequencialmente!
    if (n < 20) {
        return fib_sequencial(n);
    }

    long x, y;
    #pragma omp task shared(x)
    x = fib_paralelo(n - 1);

    #pragma omp task shared(y)
    y = fib_paralelo(n - 2);

    #pragma omp taskwait
    return x + y;
}
```

---

## 7. Grafo de Dependências com a Cláusula `depend` (OpenMP 4.0+)

Uma das inovações mais poderosas do OpenMP moderno é a capacidade de expressar **Grafos Acíclicos Dirigidos (DAG)** de tarefas sem usar barreiras de sincronização globais e travadas.

Usamos a cláusula `depend` informando a direção do fluxo de dados:
* `depend(in: var)`: A tarefa **lê** o dado de `var` (depende de tarefas anteriores que escrevem em `var`).
* `depend(out: var)`: A tarefa **escreve** no dado de `var` (tarefas futuras que leem `var` devem esperar por esta).
* `depend(inout: var)`: A tarefa **lê e escreve** em `var`.

```
+-----------------------------------------------------------------------+
|                ILUSTRAÇÃO: GRAFO DE DEPENDÊNCIAS (DAG)                |
+-----------------------------------------------------------------------+
|                                                                       |
|                       [ Tarefa A ] (out: x)                           |
|                            /       \                                  |
|                           /         \                                 |
|     (in: x, out: y) [ Tarefa B ]   [ Tarefa C ] (in: x, out: z)       |
|                           \         /                                 |
|                            \       /                                  |
|                      [ Tarefa D ] (in: y, z)                          |
|                                                                       |
+-----------------------------------------------------------------------+
```

### Exemplo Prático de Dependência:
```c
#pragma omp parallel
#pragma omp single
{
    // Tarefa 1: Produz o dado 'x'
    #pragma omp task depend(out: x)
    gerar_dados(&x);

    // Tarefa 2: Depende de 'x' e produz 'y'
    #pragma omp task depend(in: x) depend(out: y)
    processar_dados(x, &y);

    // Tarefa 3: Depende de 'x' e produz 'z' (pode rodar em PARALELO com a Tarefa 2!)
    #pragma omp task depend(in: x) depend(out: z)
    analisar_dados(x, &z);

    // Tarefa 4: Depende de 'y' e 'z'
    #pragma omp task depend(in: y, z)
    consolidar_relatorio(y, z);
}
```
O runtime do OpenMP resolve a ordem de execução **dinamicamente**. A Tarefa 2 e a Tarefa 3 rodam simultaneamente assim que a Tarefa 1 termina, sem nenhuma intervenção manual de travas ou mutexes!

---

## 8. Exemplo Prático Completo em C

Abaixo está um programa executável completo em C que demonstra o uso de tarefas para navegação em estrutura de árvore e cálculo de dependências.

```c
#include <stdio.h>
#include <stdlib.h>
#include <omp.h>

// Função para simular trabalho pesado
void fazer_trabalho(int ms) {
    double inicio = omp_get_wtime();
    while ((omp_get_wtime() - inicio) * 1000.0 < ms);
}

// 1. Exemplo de Recursão Paralela com Tarefas (Fibonacci)
long fibonacci_task(int n) {
    if (n < 2) return n;
    
    // Cutoff para evitar overhead
    if (n < 15) {
        long a = 0, b = 1, c;
        for (int i = 2; i <= n; i++) {
            c = a + b;
            a = b;
            b = c;
        }
        return b;
    }

    long x, y;

    #pragma omp task shared(x)
    x = fibonacci_task(n - 1);

    #pragma omp task shared(y)
    y = fibonacci_task(n - 2);

    #pragma omp taskwait // Espera as duas tarefas filhas terminarem
    return x + y;
}

int main() {
    int n = 30;
    long resultado;
    double inicio, fim;

    printf("=== DEMONSTRAÇÃO DE PARALELISMO DE TAREFAS NO OPENMP ===\n\n");

    // Teste 1: Fibonacci com Tarefas
    inicio = omp_get_wtime();
    #pragma omp parallel
    {
        #pragma omp single
        {
            printf("[Thread %d] Iniciando geracao de tarefas para Fibonacci(%d)...\n", 
                   omp_get_thread_num(), n);
            resultado = fibonacci_task(n);
        }
    }
    fim = omp_get_wtime();
    printf("Fibonacci(%d) = %ld | Tempo: %.4f segundos\n\n", n, resultado, fim - inicio);

    // Teste 2: Pipeline de Tarefas com Dependências (DAG)
    printf("--- Executando Grafo de Dependencias de Tarefas ---\n");
    int A = 0, B = 0, C = 0;

    inicio = omp_get_wtime();
    #pragma omp parallel
    {
        #pragma omp single
        {
            // Etapa 1
            #pragma omp task depend(out: A)
            {
                printf("[Thread %d] Etapa 1: Gerando A...\n", omp_get_thread_num());
                fazer_trabalho(100);
                A = 50;
            }

            // Etapa 2A (Depende de A)
            #pragma omp task depend(in: A) depend(out: B)
            {
                printf("[Thread %d] Etapa 2A: Processando B a partir de A (%d)...\n", omp_get_thread_num(), A);
                fazer_trabalho(100);
                B = A * 2;
            }

            // Etapa 2B (Depende de A - Roda em paralelo com 2A!)
            #pragma omp task depend(in: A) depend(out: C)
            {
                printf("[Thread %d] Etapa 2B: Processando C a partir de A (%d)...\n", omp_get_thread_num(), A);
                fazer_trabalho(100);
                C = A + 100;
            }

            // Etapa 3 (Depende de B e C)
            #pragma omp task depend(in: B, C)
            {
                printf("[Thread %d] Etapa 3: Consolidando Resultado Final (B=%d, C=%d)...\n", 
                       omp_get_thread_num(), B, C);
                printf(">> RESULTADO FINAL DA DAG: %d <<\n", B + C);
            }
        }
    }
    fim = omp_get_wtime();
    printf("Tempo total da DAG de Tarefas: %.4f segundos\n", fim - inicio);

    return 0;
}
```

---

## 9. Tabela Resumo das Diretivas de Tarefas

| Diretiva / Cláusula | Função Principal | Ponto Crítico / Observação |
| :--- | :--- | :--- |
| `#pragma omp task` | Define uma unidade assíncrona de trabalho. | Lançada na *Task Queue*; executada por qualquer thread livre. |
| `#pragma omp single` | Executa o bloco por apenas 1 thread. | Usado para semear tarefas sem duplicá-las. Possui barreira implícita. |
| `#pragma omp taskwait` | Pausa a thread até concluir tarefas filhas diretas. | Essencial para sincronizar resultados em algoritmos recursivos. |
| `#pragma omp taskgroup` | Pausa a thread até concluir tarefas filhas e todas as descendentes. | Usado em estruturas de árvore profundas ou complexas. |
| `depend(in/out/inout)` | Define dependências de dados entre tarefas. | O runtime constrói um DAG dinâmico e libera a execução automaticamente. |

---

## 10. Exercícios de Fixação

### Questão 1
Por que o código abaixo executa com erro ou produz resultados inconsistentes sem a cláusula `firstprivate` ou sem a palavra-chave `task` bem configurada em um laço `while` sobre uma lista encadeada?

```c
struct No *p = cabeca;
#pragma omp parallel
#pragma omp single
{
    while (p != NULL) {
        #pragma omp task
        processar(p); // Problema aqui!
        p = p->proximo;
    }
}
```

**Gabarito Comentado:**
Como `p` é um ponteiro compartilhado no escopo do `single`, a thread principal avança o ponteiro `p = p->proximo` rapidamente no laço `while`. Se a tarefa não fizer uma cópia privada de `p` no momento de sua criação (ou se `p` for sobrescrito antes da tarefa iniciar), múltiplas tarefas executadas por outras threads lerão o mesmo ponteiro `p` atualizado, pulando nós da lista ou processando o mesmo nó repetidamente. O uso de `firstprivate(p)` ou a declaração de um ponteiro local dentro do laço resolve o problema congelando o ponteiro de cada nó na tarefa correspondente.

---

### Questão 2
Explique a diferença funcional entre usar `#pragma omp taskwait` e usar uma barreira global `#pragma omp barrier`.

**Gabarito Comentado:**
* `#pragma omp barrier` força **TODAS** as threads do time a pararem e esperarem até que **todas** as threads alcancem aquele ponto. É uma sincronização rígida de threads.
* `#pragma omp taskwait` sincroniza **TAREFAS**, não threads. Ele apenas bloqueia a thread corrente até que as tarefas *filhas diretas* por ela criadas terminem. Enquanto uma tarefa espera no `taskwait`, a thread que a gerou pode até mesmo executar outras tarefas pendentes da fila, evitando que o hardware fique ocioso.

---

### Questão 3
O que é o "Limite de Corte" (*Cutoff Threshold*) e por que ele é obrigatório ao implementar algoritmos como o *Quicksort* ou *Fibonacci* com `#pragma omp task`?

**Gabarito Comentado:**
O limite de corte é uma condição que interrompe a criação de tarefas paralelas quando o tamanho do subproblema se torna muito pequeno. Sem ele, a quantidade de tarefas cresce exponencialmente (ex: milhões de tarefas para nós folha da árvore), e o tempo gasto pelo runtime do OpenMP criando, enfileirando e destruindo as estruturas de gerenciamento das tarefas supera imensamente o tempo do cálculo em si, destruindo o desempenho do programa.
