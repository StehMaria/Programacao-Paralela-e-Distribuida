# Aula 5.2: Escopo de Dados no OpenMP — Shared, Private, Firstprivate, Lastprivate e Default

---

## 1. Introdução: A Fronteira entre o Público e o Privado na Memória Compartilhada

Quando entramos no universo da programação paralela com **OpenMP**, a promessa principal é a simplicidade: threads executam dentro do mesmo espaço de endereçamento de memória (RAM) e podem ler e escrever em variáveis comuns [1, 2].

No entanto, essa "liberdade" é exatamente a origem do maior perigo na computação concorrente: as **Condições de Corrida (*Race Conditions*)** [3]. Se duas ou mais threads tentarem modificar a mesma variável simultaneamente sem sincronização, o resultado do programa torna-se imprevisível, incorreto e dependente do tempo de execução do processador [3, 4].

Para controlar isso, o OpenMP introduz o conceito de **Atributos de Compartilhamento de Dados (*Data-Sharing Attribute Clauses*)** [2, 5]. Através dessas cláusulas, o programador instrui o compilador sobre o ciclo de vida e a visibilidade de cada variável dentro de uma região paralela [2, 5].

---

### 💡 Analogia do Mundo Real: O Escritório Compartilhado

Pense em uma equipe de analistas (threads) trabalhando em uma sala de reuniões (região paralela) [1, 2]:

*   **`shared` (Quadro Branco da Sala):** Há um único quadro branco na parede. Tudo o que qualquer analista escreve nele fica imediatamente visível para todos os outros [2]. Se dois analistas tentarem escrever na mesma linha ao mesmo tempo com canetões de cores diferentes, o texto ficará ilegível (*race condition*) [3].
*   **`private` (Bloco de Notas Novo):** Ao entrar na reunião, cada analista recebe uma folha em branco [2, 5]. O que um escreve em seu bloco não afeta o bloco do colega. Porém, como a folha veio em branco, eles não sabem o que estava escrito no computador do chefe antes da reunião e, ao sair da sala, o bloco é jogado no lixo [5].
*   **`firstprivate` (Folha de Exercícios Fotocopiada):** Antes da reunião, o chefe tira uma foto do documento original e imprime uma cópia idêntica para cada analista [5]. Todos começam a reunião com a mesma informação inicial, mas as anotações feitas durante a reunião são individuais [5].
*   **`lastprivate` (O Relatório Final Oficial):** Durante a reunião, cada analista preenche sua folha individual correspondente à sua tarefa [5]. Ao final da última etapa sequencial da reunião, o analista responsável pela etapa final entrega sua folha para o chefe, e ela se torna o documento oficial da empresa [5].

---

## 2. Visão Geral da Memória e Alocação Interna

Em nível de sistema operacional e arquitetura de computadores, o comportamento do escopo de dados reflete o local onde as variáveis são armazenadas [1, 2]:

```
+-----------------------------------------------------------------------+
|                         MEMÓRIA COMPARTILHADA (HEAP / GLOBAL)          |
|                                                                       |
|   [ Variável SHARED ] <--- Acessada diretamente por todas as threads  |
+-----------------------------------------------------------------------+
           ^                                         ^
           |                                         |
+--------------------------+               +--------------------------+
|  STACK DA THREAD 0       |               |  STACK DA THREAD 1       |
|                          |               |                          |
|  [ Variável PRIVATE ]    |               |  [ Variável PRIVATE ]    |
|  [ Variável FIRSTPRIVATE]|               |  [ Variável FIRSTPRIVATE]|
+--------------------------+               +--------------------------+
```

*(Indicação de Imagem: Um diagrama arquitetural mostrando o Heap/Segmento de Dados contendo variáveis `shared`, e a Pilha (*Stack*) individual de cada thread alocando suas respectivas variáveis `private` e `firstprivate`).*

---

## 3. Detalhamento Cláusula por Cláusula

### 3.1 Cláusula `shared`

A cláusula `shared` declara que uma ou mais variáveis devem ser compartilhadas entre todas as threads da região paralela [2, 5].

*   **Endereço de Memória:** Todas as threads enxergam exatamente o mesmo endereço físico/virtual na memória [1, 2].
*   **Comportamento:** Alterações feitas por uma thread são imediatamente refletidas para as demais (sujeito às regras de coerência de cache) [2].
*   **Padrão no OpenMP:** Variáveis declaradas **fora** da região paralela antes do `#pragma omp parallel` são compartilhadas por padrão (`default(shared)`) [2, 5].

```c
int contador = 0;

#pragma omp parallel shared(contador)
{
    // CUIDADO: Condição de corrida! Várias threads incrementando 'contador' simultaneamente
    contador++; 
}
```

> **⚠️ Regra de Ouro da Leitura/Escrita:** Se múltiplas threads apenas **lerem** uma variável `shared`, ela é 100% thread-safe [3]. Se ao menos uma thread **escrever** em uma variável `shared`, você deve usar primitiva de sincronização (`critical`, `atomic`, `reduction` ou `mutex`) [2, 3].

---

### 3.2 Cláusula `private`

A cláusula `private` especifica que cada thread terá sua própria cópia não inicializada da variável [2, 5].

*   **Alocação:** Uma nova variável é alocada na pilha local (*stack*) de cada thread [2, 5].
*   **Armadilha 1 (Sem Inicialização):** O valor da variável antes da região paralela **NÃO** é copiado para as cópias privadas. O valor inicial das cópias privadas é **indefinido/lixo de memória** [5].
*   **Armadilha 2 (Sem Persistência):** Qualquer modificação feita na cópia privada durante a região paralela é descartada ao final. A variável original fora da região paralela mantém o valor que tinha antes da região paralela (ou permanece indefinida em algumas implementações) [5].

```c
#include <stdio.h>
#include <omp.h>

int main() {
    int x = 100;

    #pragma omp parallel private(x)
    {
        // ERRO CONCEITUAL: x aqui NÃO vale 100! Ele contém lixo de memória.
        printf("Thread %d: valor inicial de x = %d
", omp_get_thread_num(), x);
        x = omp_get_thread_num() + 1;
    }

    // Fora da região paralela: x continua valendo 100
    printf("Fora do paralelo: x = %d
", x); 
    return 0;
}
```

---

### 3.3 Cláusula `firstprivate`

A cláusula `firstprivate` é uma extensão do `private` [5]. Ela cria uma cópia privada para cada thread na sua pilha local, **mas pré-inicializa** essa cópia com o valor atual que a variável possuía imediatamente antes da região paralela [5].

*   **Quando Usar:** Quando cada thread precisa de um valor base/configuração inicial para realizar seus cálculos locais sem alterar o parâmetro original ou afetar os colegas [5].

```c
#include <stdio.h>
#include <omp.h>

int main() {
    int fator_multiplicador = 5;

    #pragma omp parallel firstprivate(fator_multiplicador)
    {
        // CORRETO: Cada thread começa com fator_multiplicador = 5!
        int id = omp_get_thread_num();
        fator_multiplicador += id; // Altera apenas a cópia local da thread
        printf("Thread %d: fator = %d
", id, fator_multiplicador);
    }

    // Fora da região paralela: fator_multiplicador original continua sendo 5
    printf("Fora do paralelo: fator original = %d
", fator_multiplicador);
    return 0;
}
```

---

### 3.4 Cláusula `lastprivate`

A cláusula `lastprivate` é utilizada em estruturas de divisão de trabalho, especificamente laços paralelos (`#pragma omp for` ou `#pragma omp parallel for`) e seções (`#pragma omp sections`) [2, 5].

Ela garante que a cópia privada da variável correspondente à **última iteração sequencial** do laço (ou última seção) seja copiada de volta para a variável original fora do bloco paralelo [5].

*   **Diferença Crucial:** Não é a "última thread a terminar no tempo real" que escreve de volta, mas sim a thread que executou a **última iteração lógica** do laço (ex: para `i = 0` até `N-1`, a iteração `N-1`) [5].

```c
#include <stdio.h>
#include <omp.h>

int main() {
    int ultimo_valor = -1;

    #pragma omp parallel for lastprivate(ultimo_valor)
    for (int i = 0; i < 100; i++) {
        // Cada thread atualiza sua cópia privada de 'ultimo_valor'
        ultimo_valor = i * 2; 
    }

    // Fora do laço: 'ultimo_valor' conterá o resultado da iteração i = 99 (99 * 2 = 198)
    printf("Valor final apos o laço: %d
", ultimo_valor); 
    return 0;
}
```

> **Nota:** É perfeitamente válido e comum combinar `firstprivate` e `lastprivate` na mesma variável: `firstprivate(x) lastprivate(x)`. Isso garante que as threads comecem com o valor de entrada e que a última iteração atualize o valor de saída [5].

---

### 3.5 Cláusula `default` e a Boa Prática de Ouro

A cláusula `default` permite alterar o comportamento padrão de escopo para variáveis não explicitamente listadas [2, 5].

*   `default(shared)`: O padrão implícito do C/C++ no OpenMP. Variáveis não declaradas explicitamente são tratadas como `shared` [2, 5].
*   `default(none)`: **A Prática de Ouro no Desenvolvimento Paralelo**. Obriga o programador a declarar **explicitamente** o escopo (`shared`, `private`, `firstprivate` ou `reduction`) de **TODAS** as variáveis utilizadas na região paralela [2, 5].

Se qualquer variável for esquecida, o compilador emitirá um **erro de compilação**, impedindo que *bugs* de *race condition* passem despercebidos para a execução [2, 3].

```c
int a = 10;
int b = 20;
int c = 0;

// COMPILATION ERROR se esquecermos de declarar 'c' com default(none):
#pragma omp parallel default(none) shared(a, b) private(c)
{
    c = a + b + omp_get_thread_num();
}
```

---

## 4. Regras Especiais e Pegadinhas Automáticas do OpenMP

Para evitar códigos verbosos, o OpenMP define escopos automáticos em situações específicas que todo desenvolvedor precisa conhecer [2, 5]:

1.  **Variáveis Declaradas Dentro do Bloco Paralelo:**
    Qualquer variável declarada dentro da região paralela ou dentro de uma função chamada dentro dela é automaticamente **`private`** (alocada na pilha local da thread que a criou) [2, 5].
    ```c
    #pragma omp parallel
    {
        int temp = 5; // Automaticamente PRIVATE para cada thread!
    }
    ```

2.  **Variável do Índice do Laço `for` Paralelizado:**
    No `#pragma omp parallel for`, a variável de controle do laço (ex: `i` em `for(int i=0;...)`) é automaticamente convertida para **`private`**, mesmo que tenha sido declarada fora do laço [2, 5].

3.  **Ponteiros e Alocação Dinâmica (Pilha vs. Heap):**
    Se um ponteiro `p` for declarado como `private`, cada thread terá sua própria **variável ponteiro**. Porém, se todas as threads inicializarem seu ponteiro privado com o mesmo endereço de memória alocado no Heap via `malloc()`, os **dados apontados continuam sendo compartilhados**!
    ```c
    int *array = (int*) malloc(100 * sizeof(int));

    #pragma omp parallel private(array) // O ponteiro 'array' é privado, mas todos apontam para o mesmo Heap!
    {
        // Risco de Race Condition nos elementos de array[i]!
    }
    ```

---

## 5. Exemplo Prático Completo em C

Abaixo está um programa demonstrativo em C que ilustra os efeitos práticos das cláusulas de escopo, comparando comportamentos incorretos e corrigidos.

```c
#include <stdio.h>
#include <stdlib.h>
#include <omp.h>

int main() {
    int valor_inicial = 42;
    int ultimo_processado = -1;
    int soma_total = 0;

    printf("=== DEMONSTRAÇÃO DE ESCOPO DE DADOS NO OPENMP ===

");

    // Definindo 4 threads para o teste
    omp_set_num_threads(4);

    #pragma omp parallel default(none)                          firstprivate(valor_inicial)                          lastprivate(ultimo_processado)                          reduction(+:soma_total)
    {
        int id = omp_get_thread_num();
        
        // Cada thread inicia com valor_inicial = 42 graças ao firstprivate
        int calculo_local = valor_inicial + (id * 10);
        printf("[Thread %d] valor_inicial recebidod: %d | calculo_local: %d
", 
               id, valor_inicial, calculo_local);

        // Divisão do laço entre as threads
        #pragma omp for
        for (int i = 0; i < 8; i++) {
            ultimo_processado = i * 100;
            printf("  -> Thread %d executou iteração i = %d (ultimo_processado = %d)
", 
                   id, i, ultimo_processado);
        }

        // Acceleração do resultado acumulado via reduction
        soma_total += calculo_local;
    }

    printf("
=== RESULTADOS FORA DA REGIÃO PARALELA ===
");
    printf("1. valor_inicial original (inalterado): %d
", valor_inicial);
    printf("2. ultimo_processado (capturado da iteração i=7 via lastprivate): %d
", ultimo_processado);
    printf("3. soma_total (combinado via reduction): %d
", soma_total);

    return 0;
}
```

---

## 6. Tabela Resumo Consolidada

| Cláusula | Cópia Privada por Thread? | Copia Valor de Entrada? | Copia Valor de Saída? | Uso Principal |
| :--- | :---: | :---: | :---: | :--- |
| **`shared`** | ❌ Não (Acesso Direto na RAM) | N/A (Mesma Memória) | N/A (Modificações Imediatas) | Leitura de matrizes/vetores grandes ou acumuladores sincronizados [2]. |
| **`private`** | 2. Sim (Alocada na Stack) | ❌ Não (Valor Inicial Lixo) | ❌ Não (Perdido ao Sair) | Variáveis temporárias de trabalho/índices auxiliares [2, 5]. |
| **`firstprivate`** | 2. Sim (Alocada na Stack) | 2. Sim (Copia Valor do Pré-Paralelo) | ❌ Não (Perdido ao Sair) | Constantes de configuração, parâmetros de entrada modificáveis localmente [5]. |
| **`lastprivate`** | 2. Sim (Alocada na Stack) | ❌ Não (Valor Inicial Lixo) | 2. Sim (Copia da Última Iteração Lógica) | Captura do estado final de laços paralelos ou seções [5]. |
| **`default(none)`** | N/A (Diretiva de Compilação) | N/A | N/A | Força o desenvolvedor a declarar todas as variáveis (Boa Prática de Segurança) [2, 5]. |

---

## 7. Exercícios de Fixação com Gabarito Comentado

### Questão 1
Considere o seguinte trecho de código em C/OpenMP:
```c
int val = 10;
#pragma omp parallel private(val)
{
    val += 5;
}
printf("%d
", val);
```
Qual será o valor impresso no `printf` fora da região paralela e por quê?
*   **A)** 15.
*   **B)** Indefinido, dependendo da thread que terminar por último.
*   **C)** 10, pois alterações em variáveis `private` são descartadas e o valor interno não é inicializado com 10.
*   **D)** Ocorre um erro de compilação.

> **Gabarito:** **C**. A cláusula `private` cria cópias locais na pilha das threads sem inicialização (contendo lixo de memória) e descarta qualquer modificação ao final do bloco. A variável `val` original fora da região paralela permanece intacta com seu valor de 10 [5].

---

### Questão 2
Para garantir que a última iteração sequencial de um laço paralelizado com `#pragma omp parallel for` preserve seu valor em uma variável declarada fora do laço após a execução, qual cláusula deve ser utilizada?
*   **A)** `firstprivate`
*   **B)** `lastprivate`
*   **C)** `shared`
*   **D)** `default(private)`

> **Gabarito:** **B**. A cláusula `lastprivate` força o OpenMP a copiar o valor da cópia privada da thread responsável pela última iteração lógica do laço de volta para a variável original fora do bloco paralelo [5].

---

### Questão 3
Por que a utilização de `default(none)` é considerada a melhor prática na programação de aplicações paralelas com OpenMP?
*   **A)** Porque aumenta automaticamente o desempenho do código em 20%.
*   **B)** Porque desativa todas as checagens de *deadlock* no compilador.
*   **C)** Porque obriga o programador a especificar explicitamente o escopo de cada variável, eliminando condições de corrida acidentais decorrentes de escopos padrão implícitos.
*   **D)** Porque aloca todas as variáveis diretamente na memória cache L1.

> **Gabarito:** **C**. O uso de `default(none)` faz com que o compilador gere um erro se qualquer variável na região paralela não tiver seu atributo de compartilhamento de dados explicitado, prevenindo que variáveis assumam o padrão `shared` e causem corrupção de dados silenciosa [2, 3, 5].
