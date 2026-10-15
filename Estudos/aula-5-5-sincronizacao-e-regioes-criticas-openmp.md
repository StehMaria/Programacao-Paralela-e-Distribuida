# Aula 5.5: Sincronização e Regiões Críticas no OpenMP — `critical`, `atomic`, `barrier`, `flush` e Travas Explícitas (`omp_lock_t`)

---

## 1. Introdução: O "Freio de Mão" Necessário do Paralelismo

Até agora no Módulo 5, aprendemos como liberar dezenas de *threads* para trabalharem juntas em alta velocidade em laços (`#pragma omp for`) e tarefas (`#pragma omp task`). No entanto, o paralelismo de memória compartilhada traz um perigo inerente: **quando múltiplas *threads* tentam alterar a mesma posição de memória ao mesmo tempo, ocorre corrupção de dados (Condição de Corrida / *Race Condition*)**.

Se o paralelismo é o pedal do acelerador de um carro de corrida, os mecanismos de **sincronização** são os freios e o volante. Sem eles, o programa roda rápido, mas colide na primeira curva!

---

### 💡 Analogia do Mundo Real: A Cozinha de Restaurante e o Forno Único

Imagine uma cozinha industrial com 8 cozinheiros (*threads*) trabalhando em paralelo preparando pratos diferentes:
* Cada cozinheiro pode picar legumes e temperar carnes na sua própria bancada sem interferir nos outros.
* **Porém, só existe 1 forno de alta temperatura na cozinha!**
* Se 3 cozinheiros tentarem colocar suas assadeiras dentro do forno exatamente ao mesmo tempo, haverá choque de assadeiras, queimaduras e pratos destruídos.
* **A Solução:** Um sistema de sinalização ou uma trava na porta do forno: apenas um cozinheiro abre e usa o forno por vez. Os outros esperam educadamente a sua vez.

---

```
  [ Threads em Execução Livre ]
     │          │          │
     ▼          ▼          ▼
  ┌───────────────────────────┐
  │   REGIAO CRITICA / LOCK   │  <-- Apenas 1 Thread entra por vez!
  └───────────────────────────┘
                │
                ▼
  [ Threads Retomam Paralelismo ]
```

---

## 2. Exclusão Mútua de Alto Nível: `#pragma omp critical`

A diretiva `#pragma omp critical` define um bloco de código como **Região Crítica**. O runtime do OpenMP garante que **apenas uma *thread* por vez** executará aquele bloco estruturado. As demais *threads* que chegarem ao ponto da crítica ficarão bloqueadas aguardando a liberação.

### 2.1 Sintaxe e Uso Básico

```c
#pragma omp parallel
{
    // Código paralelo executado por todas as threads...

    #pragma omp critical
    {
        // Apenas UMA thread por vez executa este bloco!
        saldo_global += saldo_local;
    }
}
```

---

### 2.2 Regiões Críticas Nomeadas (*Named Critical Sections*)

Por padrão, todas as diretivas `#pragma omp critical` sem nome compartilham o **mesmo trava global implícita**. Isso significa que se a Thread A entrar na `critical A` e a Thread B tentar entrar em uma `critical B` completamente independente em outro ponto do programa, a Thread B será bloqueada sem necessidade!

Para resolver isso, podemos dar **nomes** às regiões críticas:

```c
#pragma omp parallel
{
    #pragma omp critical(atualiza_banco)
    {
        contas_processadas++;
    }

    #pragma omp critical(escreve_log)
    {
        fprintf(arquivo_log, "Thread %d concluiu
", omp_get_thread_num());
    }
}
```

* **Vantagem:** `critical(atualiza_banco)` e `critical(escreve_log)` possuem travas separadas e podem ser executadas simultaneamente por *threads* diferentes!

---

## 3. Exclusão Mútua de Baixo Nível / Hardware: `#pragma omp atomic`

Embora o `#pragma omp critical` seja fácil de usar, ele carrega um **alto overhead de software** (criação e gerenciamento de travas/mutexes do SO). Se a operação que você quer proteger for uma simples atualização matemática de memória (ex: `x++` ou `soma += val`), o `#pragma omp atomic` é infinitamente superior!

O `#pragma omp atomic` instrui o compilador a utilizar **instruções atômicas do próprio hardware/CPU** (como a instrução `LOCK XADD` ou `Compare-And-Swap` do x86), realizando a atualização em um único ciclo de barramento.

---

### 3.1 Cláusulas da Diretiva `atomic`

No OpenMP moderno, podemos especificar o tipo exato de operação atômica:

1. **`update` (padrão):** Atualiza o valor de uma variável.
   ```c
   #pragma omp atomic update
   contador++;
   ```
2. **`read`:** Garante a leitura atômica de uma variável que pode estar sendo alterada por outra thread.
   ```c
   #pragma omp atomic read
   v_local = valor_global;
   ```
3. **`write`:** Garante a escrita atômica de um valor.
   ```c
   #pragma omp atomic write
   valor_global = x + 5;
   ```
4. **`capture`:** Atualiza a variável e captura seu valor original ou novo de forma atômica (equivalente ao `fetch-and-add`).
   ```c
   #pragma omp atomic capture
   meu_ticket = proximo_ticket++;
   ```

---

### 3.2 Tabela Comparativa: `#pragma omp critical` vs `#pragma omp atomic`

| Característica | `#pragma omp critical` | `#pragma omp atomic` |
| :--- | :--- | :--- |
| **Escopo de Aplicação** | Qualquer bloco arbitrário de código (múltiplas linhas, chamadas de função). | Apenas uma única instrução elementar de memória (`x++`, `x += v`, etc.). |
| **Mecanismo Interno** | Trava de software (*Mutex/Lock* do sistema operativo/runtime). | Instrução atômica de hardware (nível de montador/CPU). |
| **Overhead de Desempenho** | **Alto** (dezenas a centenas de ciclos de clock). | **Múltiplo Baixo** (poucos ciclos de clock). |
| **Flexibilidade** | Suporta estruturas de dados complexas, I/O e lógica condicional. | Restrito a escalares (inteiros, ponteiros, ponto flutuante). |

---

## 4. Sincronização de Barreiras: `#pragma omp barrier`

Uma **Barreira** é um ponto de sincronização coletiva. Quando uma *thread* atinge a diretiva `#pragma omp barrier`, ela **para e espera** até que **todas as outras *threads*** do grupo atinjam essa mesma barreira. Somente quando a última *thread* chega, a barreira se abre e todas as *threads* são liberadas juntas para a próxima etapa.

---

### 4.1 Barreiras Implícitas vs. Explícitas

* **Barreiras Implícitas:** O OpenMP insere automaticamente uma barreira invisível ao final de:
  * Um bloco `#pragma omp parallel`
  * Um laço `#pragma omp for` (a menos que a cláusula `nowait` seja usada)
  * Um bloco `#pragma omp single`
* **Barreiras Explícitas:** Inseridas pelo programador usando `#pragma omp barrier` para alinhar etapas/fases de um algoritmo iterativo.

```c
#pragma omp parallel
{
    // Fase 1: Cada thread calcula sua parte dos dados
    calcular_fase_1();

    #pragma omp barrier // <-- NINGUÉM passa daqui até que TODOS terminem a Fase 1!

    // Fase 2: Usa os resultados consolidados da Fase 1
    calcular_fase_2();
}
```

> ⚠️ **PERIGO FATAL: Deadlock com Barreiras!**  
> Nunca coloque um `#pragma omp barrier` dentro de um bloco condicional (`if`) que não seja executado por **todas** as *threads* do grupo. Se 3 *threads* entrarem no `if` e atingirem a barreira enquanto a 4ª *thread* pular o `if`, o programa ficará travado para sempre (**Deadlock**)!

---

## 5. Consistência de Memória: `#pragma omp flush`

O OpenMP adota um modelo de memória **fracamente consistente**. Isso significa que cada *thread* pode manter uma cópia temporária de variáveis nos registradores da CPU ou na memória Cache L1/L2, e mudanças feitas por uma *thread* podem **não ficar visíveis imediatamente** para as outras *threads* na memória RAM principal.

A diretiva `#pragma omp flush` estabelece um **ponto de coerência de memória** (conhecido como *Memory Fence* ou *Memory Barrier*), forçando:
1. A gravação de todas as variáveis modificadas nas caches/registradores de volta para a memória principal.
2. A invalidação das cópias locais antigas, forçando novas leituras da memória.

```c
int flag = 0;
int dados = 0;

#pragma omp parallel sections
{
    #pragma omp section
    {
        dados = 42;
        #pragma omp flush(dados, flag) // Garante que dados e flag foram escritos na RAM
        flag = 1;
        #pragma omp flush(flag)
    }

    #pragma omp section
    {
        while (flag == 0) {
            #pragma omp flush(flag) // Garante que a thread lê o valor atualizado de flag
        }
        printf("Dados recebidos: %d
", dados);
    }
}
```

> ℹ️ **Nota Prática:** A maioria das diretivas de sincronização (como `barrier`, entrada/saída de `critical`, e início/fim de regiões paralelas) já possuem um `flush` implícito embutido!

---

## 6. Sincronização Explícita Avançada: Travas OpenMP (`omp_lock_t`)

Existem situações complexas onde diretivas de alto nível (`#pragma omp critical`) não são suficientes. Por exemplo: quando você está manipulando uma **estrutura de dados dinâmica** (como um Grafo ou uma Lista Encadeada) onde cada nó individual precisa ter sua própria trava independente!

Para esses cenários, a biblioteca do OpenMP (`<omp.h>`) fornece o tipo de dado **`omp_lock_t`** e rotinas para controle explícito de travas.

---

### 6.1 Ciclo de Vida de uma Trava OpenMP

```
   [ Uninitialized ]
           │
           │  omp_init_lock()
           ▼
     [ UNLOCKED ] ◄──────────────┐
           │                     │
           │  omp_set_lock()     │  omp_unset_lock()
           ▼                     │
      [ LOCKED ] ────────────────┘
           │
           │  omp_destroy_lock()
           ▼
      [ Destroyed ]
```

---

### 6.2 Rotinas Principais de Travas

* `void omp_init_lock(omp_lock_t *lock)`: Inicializa a trava no estado desbloqueado.
* `void omp_destroy_lock(omp_lock_t *lock)`: Destrói a trava e libera recursos do sistema.
* `void omp_set_lock(omp_lock_t *lock)`: Adquire a trava (bloqueia a thread se a trava já estiver ocupada por outra).
* `void omp_unset_lock(omp_lock_t *lock)`: Libera a trava.
* `int omp_test_lock(omp_lock_t *lock)`: Tenta adquirir a trava **sem bloquear**. Retorna `1` (sucesso) ou `0` (falha). Excelente para evitar espera ociosa!

---

## 7. Exemplo Prático Completo em C

Abaixo está um programa completo e executável que reúne todas as primitivas de sincronização aprendidas nesta aula.

```c
#include <stdio.h>
#include <stdlib.h>
#include <omp.h>

#define NUM_ELEMENTOS 1000000

int main() {
    long soma_critical = 0;
    long soma_atomic = 0;
    omp_lock_t trava_impressao;

    // Inicializa a trava explícita
    omp_init_lock(&trava_impressao);

    printf("=== DEMONSTRACAO DE SINCRONIZACAO EM OPENMP ===

");

    #pragma omp parallel num_threads(4)
    {
        int tid = omp_get_thread_num();

        // 1. Uso de #pragma omp critical para relatórios ordenados
        #pragma omp critical(impressao_inicial)
        {
            printf("[Thread %d] Iniciou a fase 1.
", tid);
        }

        // 2. Acumulo com #pragma omp atomic (Velocidade máxima!)
        #pragma omp for
        for (int i = 0; i < NUM_ELEMENTOS; i++) {
            #pragma omp atomic
            soma_atomic += 1;
        }

        // 3. Ponto de Sincronização Coletiva (Barreira)
        #pragma omp barrier

        // 4. Uso de omp_lock_t para acesso exclusivo explícito
        omp_set_lock(&trava_impressao);
        printf("[Thread %d] Passou da barreira e adquiriu o Lock explícito!
", tid);
        omp_unset_lock(&trava_impressao);

        // 5. Acumulo com #pragma omp critical (Demonstração de equivalência)
        #pragma omp for
        for (int i = 0; i < NUM_ELEMENTOS; i++) {
            #pragma omp critical(soma_global)
            {
                soma_critical += 1;
            }
        }
    }

    // Destrói a trava explícita
    omp_destroy_lock(&trava_impressao);

    printf("
=== RESULTADOS FINAIS ===
");
    printf("Soma com ATOMIC   : %ld (Esperado: %d)
", soma_atomic, NUM_ELEMENTOS);
    printf("Soma com CRITICAL : %ld (Esperado: %d)
", soma_critical, NUM_ELEMENTOS);

    return 0;
}
```

---

## 8. Síntese do Módulo 5: Resumo e Mapa Mental de OpenMP

Com esta aula, concluímos o **Módulo 5 (Programação com Memória Compartilhada - OpenMP)**! Vamos recalibrar a nossa caixa de ferramentas:

```
                          ┌─────────────────────────────────────┐
                          │         OPENMP FRAMEWORK            │
                          └──────────────────┬──────────────────┘
                                             │
    ┌─────────────────────────┬──────────────┴──────────────┬─────────────────────────┐
    │                         │                             │                         │
    ▼                         ▼                             ▼                         ▼
[ Regiões Paralelas ]     [ Trabalho Paralelo ]     [ Escopo de Dados ]      [ Sincronização ]
• #pragma omp parallel    • #pragma omp for         • shared                 • critical
• Modelo Fork-Join        • #pragma omp task        • private                • atomic
• Master/Worker Threads   • schedule(static|dynamic)• firstprivate           • barrier
                          • depend(in|out)          • default(none)          • omp_lock_t
```

---

## 9. Exercícios de Fixação com Gabarito Comentado

### Questão 1
**Um programador deseja atualizar uma variável global do tipo contador (`contador++`) dentro de um laço paralelo executado por 16 *threads*. Qual das seguintes abordagens é a mais eficiente em termos de desempenho?**
* a) Envolver `contador++` em um `#pragma omp critical`.
* b) Utilizar a diretiva `#pragma omp atomic update` antes de `contador++`.
* c) Utilizar `#pragma omp barrier` antes de cada incremento.
* d) Declarar `contador` como `firstprivate`.

* **Gabarito:** **b**. O `#pragma omp atomic update` traduz o incremento para uma única instrução de hardware de baixo overhead, enquanto o `critical` utiliza um mutex de software muito mais pesado.

---

### Questão 2
**Considere o seguinte trecho de código OpenMP:**
```c
#pragma omp parallel
{
    if (omp_get_thread_num() % 2 == 0) {
        #pragma omp barrier
    }
}
```
**O que acontecerá durante a execução deste programa quando executado com 4 *threads*?**
* a) O programa executará normalmente e apenas as *threads* pares serão sincronizadas.
* b) Ocorrerá um erro de compilação, pois `barrier` não pode ser usada dentro de um `if`.
* c) O programa entrará em **Deadlock**, pois as *threads* ímpares nunca atingirão a barreira, fazendo com que as *threads* pares esperem indefinidamente.
* d) O OpenMP criará automaticamente duas sub-barreiras separadas.

* **Gabarito:** **c**. Barreiras só funcionam se **todas** as *threads* do grupo atingirem o ponto de sincronização. Se apenas as *threads* pares entrarem no `if`, elas ficarão bloqueadas para sempre aguardando as *threads* ímpares.

---

### Questão 3
**Qual é a principal vantagem de utilizar regiões críticas nomeadas (`#pragma omp critical(nome)`) em relação a regiões críticas padrão sem nome?**
* a) Aumenta a velocidade de compilação do código.
* b) Permite que regiões críticas com nomes diferentes sejam executadas simultaneamente por *threads* distintas, reduzindo contenções desnecessárias.
* c) Garante que a memória seja limpa automaticamente.
* d) Transforma a região crítica em uma instrução atômica de hardware.

* **Gabarito:** **b**. Regiões críticas sem nome compartilham a mesma trava global. Ao atribuir nomes diferentes, criamos travas independentes, permitindo concorrência entre seções que não disputam o mesmo recurso.
