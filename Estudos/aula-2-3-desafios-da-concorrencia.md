# Aula 2.3: Desafios da Concorrência — Condições de Corrida, Deadlocks, Livelocks e Starvation

## Introdução: O Lado Sombrio da Concorrência

Nas aulas anteriores, vimos como o Sistema Operacional e o hardware nos permitem executar múltiplas *threads* e processos em paralelo. Parece o cenário ideal: mais braços trabalhando juntos para resolver um problema mais rápido!

No entanto, quando colocamos múltiplos executores para manipular **os mesmos dados ao mesmo tempo**, entramos num território perigoso. Se não houver coordenação estrita, o comportamento do software deixa de ser previsível e determinístico. 

Erros de concorrência são famosos por serem **Heisenbugs** — bugs fantasmas que aparecem durante o uso normal do cliente, mas somem assim que você tenta rodar o programa no depurador (*debugger*) ou adiciona comandos `printf` para investigar.

Nesta aula, vamos estudar os quatro maiores vilões do desenvolvimento concorrente:
1. **Condições de Corrida (*Race Conditions*)**
2. **Impasses Fatais (*Deadlocks*)**
3. **Bloqueios Ativos (*Livelocks*)**
4. **Inanição (*Starvation*)**

---

## 1. Condição de Corrida (*Race Condition*) & Região Crítica

### A Analogia da Conta Bancária Conjunta
Imagine um casal que possui uma conta bancária conjunta com saldo inicial de **R$ 100,00**. 

No mesmo segundo, a pessoa A vai ao caixa eletrônico do Bairro Ocidental sacar R$ 50,00, enquanto a pessoa B usa o aplicativo no celular para depositar R$ 50,00.

Se o sistema do banco não tiver sincronização:
1. O Caixa Eletrônico lê o saldo: `R$ 100,00`.
2. O Celular lê o saldo: `R$ 100,00`.
3. O Celular calcula `100 + 50 = 150` e grava na conta. Saldo atual: **R$ 150,00**.
4. Um milissegundo depois, o Caixa Eletrônico calcula `100 - 50 = 50` e grava na conta. Saldo final: **R$ 50,00**!

O depósito de R$ 50,00 simplesmente "desapareceu no ar" porque a escrita do Caixa Eletrônico **sobrescreveu** a escrita do Celular.

```
       [ Thread A (Caixa) ]                        [ Thread B (Celular) ]
              │                                               │
  (1) Lê saldo (R$ 100)                                       │
              │                                    (2) Lê saldo (R$ 100)
  (3) Subtrai 50 -> R$ 50                                     │
              │                                    (4) Soma 50 -> R$ 150
  (5) Escreve R$ 50 na RAM                                    │
              │                                    (6) Escreve R$ 150 na RAM
              ▼                                               ▼
   ========================================================================
   RESULTADO FINAL NA MEMÓRIA: R$ 150 (ou R$ 50, dependendo da ordem!)
   RESULTADO CORRETO ESPERADO: R$ 100 + 50 - 50 = R$ 100
   ========================================================================
```

### Por que isso acontece no nível do Hardware?
Em linguagem C, o comando `saldo += 50;` parece ser uma única linha de código. No entanto, em linguagem de montagem (*Assembly* x86/ARM), essa instrução é decomposta em **três passos não-atômicos**:

1. **READ:** Carrega o valor da RAM para um registrador da CPU (`mov eax, [saldo]`).
2. **MODIFY:** Incrementa o valor no registrador (`add eax, 50`).
3. **WRITE:** Grava o valor do registrador de volta na RAM (`mov [saldo], eax`).

Se o escalonador do Sistema Operacional interromper a *thread* (fazendo uma troca de contexto) entre o passo 1 e o passo 3, outra *thread* pode ler o valor antigo da RAM, gerando a **condição de corrida**.

> **Região Crítica (*Critical Section*):** É qualquer trecho de código que acessa um recurso compartilhado (como uma variável global ou arquivo) onde pelo menos uma das *threads* realiza uma operação de **escrita**.

---

### Exemplo Prático em C: O Erro e a Solução

#### Código COM Condição de Corrida (Incorreto)

```c
#include <stdio.h>
#include <pthread.h>

#define NUM_THREADS 4
#define INCREMENTOS 1000000

long int contador_global = 0; // Recurso compartilhado

void* incrementar(void* arg) {
    for (int i = 0; i < INCREMENTOS; i++) {
        contador_global++; // ATENÇÃO: Operação não-atômica!
    }
    return NULL;
}

int main() {
    pthread_t threads[NUM_THREADS];

    // Cria 4 threads
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_create(&threads[i], NULL, incrementar, NULL);
    }

    // Aguarda todas terminarem
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    // Esperado: 4 * 1.000.000 = 4.000.000
    printf("Resultado Final do Contador: %ld
", contador_global);
    return 0;
}
```
*Se você executar o código acima, o valor impresso será quase imprevisível (ex: `1843201`, `2104590`), variando a cada execução!*

#### Código CORRIGIDO usando Mutex (Exclusão Mútua)

```c
#include <stdio.h>
#include <pthread.h>

#define NUM_THREADS 4
#define INCREMENTOS 1000000

long int contador_global = 0;
pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER; // Declaração da trava

void* incrementar_seguro(void* arg) {
    for (int i = 0; i < INCREMENTOS; i++) {
        pthread_mutex_lock(&trava);   // --- ENTRADA NA REGIÃO CRÍTICA ---
        contador_global++;
        pthread_mutex_unlock(&trava); // --- SAÍDA DA REGIÃO CRÍTICA ---
    }
    return NULL;
}

int main() {
    pthread_t threads[NUM_THREADS];

    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_create(&threads[i], NULL, incrementar_seguro, NULL);
    }

    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    printf("Resultado Seguro do Contador: %ld
", contador_global); // Sempre 4000000!
    pthread_mutex_destroy(&trava);
    return 0;
}
```

---

## 2. Deadlock (Impasse Fatal)

### A Analogia do Cruzamento Trancado
Imagine um cruzamento em uma cidade onde quatro carros chegam ao mesmo tempo em quatro vias perpendiculares. 
* O carro do Norte quer avançar para o Sul, mas precisa do espaço ocupado pelo carro do Leste.
* O carro do Leste precisa do espaço do carro do Sul.
* O carro do Sul precisa do espaço do carro do Oeste.
* O carro do Oeste precisa do espaço do carro do Norte.

Nenhum carro pode dar a ré, nenhum carro pode passar por cima do outro. O trânsito para **completamente para sempre**. Isso é um **Deadlock**.

```
                   [ Carro Norte ]
                         │  espera por
                         ▼
  [ Carro Oeste ] ◄─────────────── [ Carro Leste ]
     espera por   │  espera por
                  ▼
            [ Carro Sul ]
```

### As 4 Condições de Coffman
Para que um Deadlock ocorra em um sistema computacional, **todas as 4 condições a seguir precisam ser verdadeiras simultaneamente** (identificadas por Edward G. Coffman Jr. em 1971):

1. **Exclusão Mútua:** O recurso não pode ser compartilhado simultaneamente; apenas uma *thread* pode segurá-lo por vez.
2. **Posse e Espera (*Hold and Wait*):** Uma *thread* segura um recurso enquanto está parada aguardando a liberação de outro recurso.
3. **Não-Preempção:** Um recurso não pode ser tirado à força de uma *thread*; ele só é liberado voluntariamente por quem o segura.
4. **Espera Circular (*Circular Wait*):** Existe uma corrente fechada de *threads* $T_1, T_2, \dots, T_n$, onde $T_1$ espera por um recurso retido por $T_2$, $T_2$ espera por $T_3$, e $T_n$ espera por $T_1$.

---

### Exemplo de Deadlock em C

```c
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

pthread_mutex_t mutex_A = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_B = PTHREAD_MUTEX_INITIALIZER;

// Thread 1: Pega A, depois tenta pegar B
void* funcao_thread_1(void* arg) {
    pthread_mutex_lock(&mutex_A);
    printf("Thread 1: Bloqueou Mutex A. Tentando pegar Mutex B...
");
    sleep(1); // Simula algum trabalho e força o entrelaçamento
    
    pthread_mutex_lock(&mutex_B); // Ficará travada aqui!
    printf("Thread 1: Conseguiu os dois mutexes!
");
    
    pthread_mutex_unlock(&mutex_B);
    pthread_mutex_unlock(&mutex_A);
    return NULL;
}

// Thread 2: Pega B, depois tenta pegar A
void* funcao_thread_2(void* arg) {
    pthread_mutex_lock(&mutex_B);
    printf("Thread 2: Bloqueou Mutex B. Tentando pegar Mutex A...
");
    sleep(1);
    
    pthread_mutex_lock(&mutex_A); // Ficará travada aqui!
    printf("Thread 2: Conseguiu os dois mutexes!
");
    
    pthread_mutex_unlock(&mutex_A);
    pthread_mutex_unlock(&mutex_B);
    return NULL;
}

int main() {
    pthread_t t1, t2;
    pthread_create(&t1, NULL, funcao_thread_1, NULL);
    pthread_create(&t2, NULL, funcao_thread_2, NULL);

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    return 0; // NUNCA CHEGARÁ AQUI!
}
```

### Como prevenir ou quebrar o Deadlock?
A técnica mais simples e eficaz para evitar deadlocks é a **Hierarquia de Recursos (Ordenação Global de Trava)**:
* Estabeleça uma regra universal: **Sempre adquira os *mutexes* na mesma ordem global**.
* Se a Thread 1 e a Thread 2 ambas pegarem primeiro o `mutex_A` e só depois o `mutex_B`, a Espera Circular é matematicamente eliminada!

---

## 3. Livelock (Bloqueio Ativo)

### A Analogia das Pessoas no Corredor
Imagine duas pessoas muito educadas caminhando em um corredor estreito em sentidos opostos. 
Elas se encontram frente a frente:
1. As duas tentam se desviar para a **esquerda** ao mesmo tempo. Continuam bloqueadas.
2. As duas se desculpam e tentam se desviar para a **direita** ao mesmo tempo. Continuam bloqueadas.
3. Elas repetem esse movimento infinitamente a uma velocidade incrível!

Perceba a diferença crítica em relação ao Deadlock:
* No **Deadlock**, as pessoas ficam paradas, estáticas, congeladas (Estado: `BLOCKED/SLEEPING`, Uso de CPU: **0%**).
* No **Livelock**, as pessoas estão se movimentando freneticamente, gastando energia e mudando de estado continuamente, mas sem realizar progresso útil algum (Estado: `RUNNING`, Uso de CPU: **100%**)!

```
   [ DEADLOCK ]                             [ LIVELOCK ]
   Threads: Paradas / Dormindo              Threads: Rodando em loop ativo
   Estado: BLOCKED                          Estado: RUNNING
   CPU: 0% de uso                           CPU: 100% de uso (Desperdício)
   Progresso: NENHUM                        Progresso: NENHUM
```

### Como resolver Livelocks?
Adicionar **aleatoriedade (*random backoff*)**. É exatamente assim que o protocolo de rede Ethernet trata colisões de pacotes: se dois nós tentam transmitir ao mesmo tempo, em vez de tentarem novamente em intervalos fixos idênticos, cada um aguarda um tempo aleatório antes de tentar novamente.

---

## 4. Starvation (Inanição / Fome Indefinida)

### A Analogia do Balcão de Atendimento sem Fila
Imagine um balcão de lanchonete onde não existe fila por ordem de chegada. O atendente decide atender sempre a pessoa mais alta e mais barulhenta que estiver na frente do balcão.

Uma pessoa quieta e de baixa estatura chega para pedir um sanduíche. Todas as vezes que o atendente se liberta, clientes mais altos chegam e tomam a frente. A pessoa quieta não está em deadlock (ela não está travada por ninguém), mas ela **nunca é atendida**. Ela morre de fome (*starvation*).

```
   [ Fila do Sistema Operacional / Recurso ]
   
   ( Alta Prioridade ) ---> [ Processador ]  ---> (Atendido!)
   ( Alta Prioridade ) ---> [ Processador ]  ---> (Atendido!)
   ( Alta Prioridade ) ---> [ Processador ]  ---> (Atendido!)
   ...
   [ Thread Baixa Prioridade ] ---> (Aguardando indefinidamente na fila...)
```

### Por que ocorre em Software?
1. **Políticas Injustas de Escalonamento:** Algoritmos de escalonamento focados estritamente em prioridade estática sempre passam *threads* de alta prioridade na frente.
2. **Escassez de Recursos por Concorrência Feroz:** *Threads* gananciosas adquirem uma trava de exclusão mútua, fazem o processamento e liberam o *mutex*, mas imediatamente tentam adquirir o mesmo *mutex* em um laço rápido antes que *threads* adormecidas consigam acordar.

### Como evitar Starvation?
* **Filas FIFO (*First-In, First-Out*):** Garantem que a *thread* que está esperando há mais tempo seja atendida primeiro (mecanismos *Fair Locks*).
* **Envelhecimento (*Aging*):** O Sistema Operacional aumenta gradualmente a prioridade de uma *thread* quanto mais tempo ela permanece na fila de espera.

---

## Tabela Comparativa de Diagnóstico

| Métrica / Sintoma | **Race Condition** | **Deadlock** | **Livelock** | **Starvation** |
| :--- | :--- | :--- | :--- | :--- |
| **Sintoma Visual** | Dados corrompidos / Erros aleatórios no resultado. | Aplicação trava totalmente e não responde. | Aplicação trava o progresso, mas a CPU vai a 100%. | Apenas um módulo ou thread específica para de responder. |
| **Estado da Thread** | `RUNNING` (mas fora de ordem). | `WAITING` / `BLOCKED`. | `RUNNING` (loop infinito). | `READY` ou `WAITING`. |
| **Consumo de CPU** | Normal. | **0%** (parado). | **100%** (desperdício alto). | Normal/Variável. |
| **Causa Raiz** | Falta de exclusão mútua em variável compartilhada. | Espera circular por múltiplos recursos. | Mudanças ativas de estado sem coordenação/aleatoriedade. | Algoritmos de escalonamento ou travas injustas. |
| **Solução Principal** | Usar Mutex, Semáforo ou Instruções Atômicas. | Ordenação global de aquisição de travas (*Lock Ordering*). | Adicionar aleatoriedade (*Randomized Backoff*). | Filas Fair (FIFO) e mecanismo de Envelhecimento (*Aging*). |

---

## Resumo da Aula e Boas Práticas

1. Sempre identifique o **estado compartilhado** do seu programa e proteja as **regiões críticas** usando primitivas de sincronização apropriadas.
2. Evite ao máximo segurar múltiplos *locks* ao mesmo tempo. Se for inevitável, **respeite sempre a mesma ordem de aquisição** para eliminar a possibilidade de Deadlocks.
3. Não confie em testes simples para validar programas concorrentes: o fato de um código passar 100 vezes sem erros nos seus testes não garante a ausência de *Race Conditions*.
4. Prefira modelos de programação de mais alto nível (como OpenMP ou concorrência por troca de mensagens) sempre que possível, reduzindo a necessidade de gerenciar *mutexes* manuais.
