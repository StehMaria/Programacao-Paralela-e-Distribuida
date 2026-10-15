# Aula 2.4: Primitivas de Sincronização e Exclusão Mútua — Mutexes, Semáforos, Spinlocks, Barreiras e Variáveis de Condição

---

## **Introdução: A Necessidade de Regras de Trânsito no Código Concorrente**

Na **Aula 2.3**, vimos o caos que acontece quando múltiplas *threads* tentam acessar e modificar a mesma posição de memória sem coordenação: dados corrompidos (*race conditions*), travamentos perpétuos (*deadlocks*), e *threads* presas em loops sem fim (*livelocks*).

Para transformar esse caos em uma execução paralela harmoniosa, os sistemas operacionais e bibliotecas de *threads* oferecem as **Primitivas de Sincronização**. 

> 💡 **Analogia da Festa e dos Banheiros:**  
> Imagine uma festa grande com 100 convidados (*threads*) e um único banheiro com chave (*recurso compartilhado*).  
> * Sem regras, vários convidados tentariam entrar ao mesmo tempo, gerando confusão (*Race Condition*).  
> * O **Mutex** é como a chave única da porta: quem pega a chave entra, tranca a porta por dentro e só devolve a chave ao sair.  
> * O **Semáforo** é como o controle de acesso de uma boate: ele permite até N pessoas entrarem simultaneamente.  
> * O **Spinlock** é como uma pessoa impaciente que fica batendo na porta a cada segundo perguntando: *"Já saiu? Já saiu?"* (*espera ocupada*).  
> * A **Barreira** é como um ônibus de excursão que só dá a partida quando **todos** os passageiros estiverem sentados.  
> * A **Variável de Condição** é como a senha de uma lanchonete: a pessoa senta e aguarda confortavelmente até que o painel chame o seu número.

---

## **Estrutura Visual do Mapeamento de Primitivas**

```
┌───────────────────────────────────────────────────────────────────────────────────┐
│                          PRIMITIVAS DE SINCRONIZAÇÃO                              │
├───────────────────────────────┬───────────────────────────────────────────────────┤
│ Exclusão Mútua (1 recurso)    │  ► Mutex (Bloqueia/Dorme)                         │
│                               │  ► Spinlock (Espera Ocupada / High CPU)           │
├───────────────────────────────┼───────────────────────────────────────────────────┤
│ Sinalização e Vagas (N recs)  │  ► Semáforo Binário / Contador                    │
├───────────────────────────────┼───────────────────────────────────────────────────┤
│ Sincronização em Fases        │  ► Barreira (Ponto de encontro para N threads)    │
├───────────────────────────────┼───────────────────────────────────────────────────┤
│ Espera por Condição Lógica    │  ► Variável de Condição + Mutex                   │
└───────────────────────────────┴───────────────────────────────────────────────────┘
```

---

## **1. Mutex (*Mutual Exclusion*) — A Chave de Posse Exclusiva**

### **O que é e como funciona?**
O **Mutex** (Abreviação de *Mutual Exclusion*) é a primitiva mais fundamental para proteger uma **Região Crítica**. Ele funciona como uma trava binária com estados simples: **LOCKED** (Travado) ou **UNLOCKED** (Destravado).

#### **A Regra de Ouro do Mutex: Conceito de Posse (*Ownership*)**
Diferente de outras primitivas, o Mutex possui o conceito estrito de **propriedade**: **Apenas a *thread* que adquiriu (destravou para si) o Mutex tem a permissão de destravá-lo (*unlock*)**. Se a *Thread A* der o `lock`, a *Thread B* **não pode** dar o `unlock`.

#### **O Fluxo de Execução:**
1. A *Thread A* executa `pthread_mutex_lock(&mutex)`. Como estava livre, ela assume o Mutex e entra na região crítica.
2. A *Thread B* tenta executar `pthread_mutex_lock(&mutex)`. Como o Mutex está ocupado, o SO coloca a *Thread B* no estado **BLOCKED/WAITING** (dormindo) e libera a CPU para outros processos.
3. Quando a *Thread A* conclui o trabalho, executa `pthread_mutex_unlock(&mutex)`.
4. O SO acorda a *Thread B*, que agora adquire a trava e prossegue.

---

### **Exemplo Prático em C (Pthreads): Incremento Seguro com Mutex**

```c
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#define NUM_THREADS 4
#define INCREMENTOS 1000000

long contador_compartilhado = 0;

// Declaração do Mutex
pthread_mutex_t trava;

void* incrementar(void* arg) {
    for (int i = 0; i < INCREMENTOS; i++) {
        // --- INÍCIO DA REGIÃO CRÍTICA ---
        pthread_mutex_lock(&trava);
        
        contador_compartilhado++; // Operação protegida contra Race Condition
        
        pthread_mutex_unlock(&trava);
        // --- FIM DA REGIÃO CRÍTICA ---
    }
    return NULL;
}

int main() {
    pthread_t threads[NUM_THREADS];

    // Inicialização do Mutex
    pthread_mutex_init(&trava, NULL);

    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_create(&threads[i], NULL, incrementar, NULL);
    }

    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    // Destruição do Mutex
    pthread_mutex_destroy(&trava);

    printf("Valor final do contador: %ld (Esperado: %d)
", 
           contador_compartilhado, NUM_THREADS * INCREMENTOS);

    return 0;
}
```

---

## **2. Semáforos — Os Cartões de Acesso e Sinalização**

Criado pelo cientista da computação **Edsger Dijkstra**, o **Semáforo** é uma variável inteira atômica protegida que gerencia a disponibilidade de um número N de recursos.

### **As Duas Operações Atômicas Fundamentais:**
*   **`sem_wait(s)` (conhecida como P ou Down):** Decrementa o valor do semáforo em 1. Se o valor resultante for menor que 0, a *thread* é bloqueada e dorme até que o recurso seja liberado.
*   **`sem_post(s)` (conhecida como V ou Up):** Incrementa o valor do semáforo em 1. Se houver *threads* dormindo na fila, o SO acorda uma delas.

```
                  ┌──────────────────────────────┐
                  │ Contador do Semáforo (val)   │
                  └──────────────┬───────────────┘
                                 │
           ┌─────────────────────┴─────────────────────┐
           ▼                                           ▼
   sem_wait() [Decrementa]                    sem_post() [Incrementa]
   If val < 0: Dorme na fila                  Acorda uma thread se houver
```

### **Tipos de Semáforos:**
1.  **Semáforo Binário (Valor Inicial = 1):** Funciona de forma similar ao Mutex (0 ou 1), mas **NÃO possui o conceito de posse (*ownership*)**. Qualquer *thread* pode emitir um `sem_post()`.
2.  **Semáforo Contador (*Counting Semaphore*, Valor Inicial = N):** Permite controlar o acesso simultâneo a um conjunto com N recursos idênticos (ex: limite de 5 conexões em um banco de dados).

---

### **Exemplo Prático em C: Semáforo Contador para Vagas de Estacionamento**

```c
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>

#define VAGAS_NO_ESTACIONAMENTO 2
#define TOTAL_CARROS 5

sem_t semaforo_vagas;

void* carro(void* arg) {
    int id = *(int*)arg;

    printf("Carro %d: Procurando vaga...
", id);

    // Tenta pegar uma vaga (decrementa o semáforo)
    sem_wait(&semaforo_vagas);

    printf("Carro %d: ESTACIONOU (Vaga garantida)!
", id);
    sleep(1); // Simula o tempo estacionado

    printf("Carro %d: Saindo da vaga...
", id);
    
    // Libera a vaga (incrementa o semáforo)
    sem_post(&semaforo_vagas);

    free(arg);
    return NULL;
}

int main() {
    pthread_t threads[TOTAL_CARROS];

    // Inicializa o semáforo com 2 vagas (compartilhado entre threads = 0)
    sem_init(&semaforo_vagas, 0, VAGAS_NO_ESTACIONAMENTO);

    for (int i = 0; i < TOTAL_CARROS; i++) {
        int* id = malloc(sizeof(int));
        *id = i + 1;
        pthread_create(&threads[i], NULL, carro, id);
    }

    for (int i = 0; i < TOTAL_CARROS; i++) {
        pthread_join(threads[i], NULL);
    }

    sem_destroy(&semaforo_vagas);
    return 0;
}
```

---

## **3. Spinlocks — A Espera Ocupada (*Busy-Waiting*)**

### **O que é e qual a diferença para o Mutex?**
Quando uma *thread* tenta adquirir um Mutex ocupado, o Kernel realiza uma **Troca de Contexto (*Context Switch*)** colocando a *thread* para dormir. Essa operação de suspender e depois acordar a *thread* custa milhares de ciclos de CPU.

O **Spinlock** resolve isso de outra maneira: ele **NÃO faz a *thread* dormir**. Em vez disso, a *thread* permanece em um loop `while` contínuo no nível de instrução do processador (usando instruções atômicas como `Test-and-Set` do hardware), verificando se a trava foi liberada.

```c
// Lógica interna conceitual de um Spinlock
void spin_lock(spinlock_t *lock) {
    while (atomic_test_and_set(lock) == OCUPADO) {
        // Não dorme! Fica rodando no loop consumindo CPU ativamente
    }
}
```

### **Quando usar Spinlocks?**
*   **Vantagem:** Latência de resposta ultrabaixa. Quando a trava é liberada, a *thread* a pega **instantaneamente**, sem esperar o Kernel do SO acordá-la.
*   **Desvantagem:** Consome 100% da capacidade de um núcleo de CPU durante a espera.
*   **Indicação Ideal:** Seções críticas **extremamente curtas** (ex: alterar poucas variáveis), onde o tempo gasto esperando a trava é menor que o custo de uma Troca de Contexto. Usado ostensivamente em núcleos de Sistemas Operacionais e drivers de hardware.

---

### **Exemplo em C com Pthreads Spinlock**

```c
#include <stdio.h>
#include <pthread.h>

pthread_spinlock_t spinlock;
long saldo = 0;

void* atualizar_saldo(void* arg) {
    for (int i = 0; i < 100000; i++) {
        pthread_spin_lock(&spinlock); // Espera ocupada ultrarrápida
        saldo += 1;
        pthread_spin_unlock(&spinlock);
    }
    return NULL;
}

int main() {
    pthread_t t1, t2;

    // Inicialização do Spinlock
    pthread_spin_init(&spinlock, PTHREAD_PROCESS_PRIVATE);

    pthread_create(&t1, NULL, atualizar_saldo, NULL);
    pthread_create(&t2, NULL, atualizar_saldo, NULL);

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    pthread_spin_destroy(&spinlock);

    printf("Saldo Final: %ld
", saldo);
    return 0;
}
```

---

## **4. Barreiras (*Barriers*) — Ponto de Encontro e Sincronização em Fases**

### **O que é uma Barreira?**
Uma **Barreira** é um ponto de sincronização coletiva. Ela é configurada com um número N de *threads*. Nenhuma *thread* consegue avançar além da barreira até que **todas as N *threads*** tenham chegado a esse mesmo ponto.

```
Thread 1 ──────► [ B A R R E I R A ] (Espera...)  ───────► Continua
Thread 2 ────────────► [ B A R R E I R A ] (Espera...) ──► Continua
Thread 3 ──────────────────► [ B A R R E I R A ] (Chegou a última!) ──► Todas liberadas!
```

### **Caso de Uso Típico:**
Algoritmos paralelos iterativos divididos em fases (ex: simulação de dinâmica de fluidos ou multiplicação de matrizes por blocos). A Fase 2 só pode começar quando todas as *threads* terminarem o cálculo da Fase 1.

---

### **Exemplo Prático em C: Sincronização por Fases**

```c
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#define NUM_THREADS 3

pthread_barrier_t barreira;

void* worker(void* arg) {
    long id = (long)arg;

    printf("Thread %ld: Finalizou a Etapa 1.
", id);
    sleep(id + 1); // Simula tempos de trabalho diferentes

    printf("Thread %ld: Chegou na barreira e aguarda as outras...
", id);
    
    // Todas esperam aqui!
    pthread_barrier_wait(&barreira);

    printf("Thread %ld: PASSOU da barreira! Iniciando Etapa 2.
", id);
    return NULL;
}

int main() {
    pthread_t threads[NUM_THREADS];

    // Inicializa a barreira para esperar exatamente 3 threads
    pthread_barrier_init(&barreira, NULL, NUM_THREADS);

    for (long i = 0; i < NUM_THREADS; i++) {
        pthread_create(&threads[i], NULL, worker, (void*)i);
    }

    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    pthread_barrier_destroy(&barreira);
    return 0;
}
```

---

## **5. Variáveis de Condição (*Condition Variables*) — Sinalização de Eventos**

### **O que é e por que é necessária?**
Muitas vezes, uma *thread* precisa esperar até que **uma condição lógica específica seja verdadeira** (ex: *"só posso processar se a fila de tarefas NÃO estiver vazia"*).

Fazer isso com um laço enquanto verifica o estado consume toda a CPU. A **Variável de Condição (*Condition Variable* ou CondVar)** permite que a *thread* durma eficientemente enquanto aguarda a notificação de que o estado mudou.

#### **A Regra Indissociável: CondVar SEMPRE Trabalha com um Mutex!**
A operação `pthread_cond_wait(&cond, &mutex)` realiza **três ações atômicas de uma só vez**:
1. Libera o Mutex associado.
2. Coloca a *thread* para dormir.
3. Ao acordar (quando recebe o sinal), ela **re-adquire automaticamente o Mutex** antes de continuar a execução.

---

### **Por que DEVE-SE usar `while` e NÃO `if` na verificação?**

```c
// ❌ INCORRETO (Vulnerável a Acordares Espúrios / Spurious Wakeups)
pthread_mutex_lock(&mutex);
if (fila_vazia) {
    pthread_cond_wait(&cond, &mutex);
}
// Pode falhar se duas threads acordarem juntas e a primeira esvaziar a fila!
item = retirar_da_fila(); 
pthread_mutex_unlock(&mutex);

// ✅ CORRETO (Padrão de Projeto Robusto)
pthread_mutex_lock(&mutex);
while (fila_vazia) { 
    pthread_cond_wait(&cond, &mutex); // Re-testa a condição ao acordar!
}
item = retirar_da_fila();
pthread_mutex_unlock(&mutex);
```

> ⚠️ **Motivo:** O sistema operacional pode gerar **acordares espúrios** (*spurious wakeups*), acordando a *thread* mesmo sem o envio explícito de um sinal. O laço `while` garante que a condição lógica seja reavaliada antes de prosseguir.

---

### **Exemplo Prático: Produtor e Consumidor em C**

```c
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

int dados_disponiveis = 0;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond_var = PTHREAD_COND_INITIALIZER;

void* consumidor(void* arg) {
    pthread_mutex_lock(&mutex);

    // Usa WHILE para prevenir Spurious Wakeup
    while (dados_disponiveis == 0) {
        printf("Consumidor: Fila vazia. Dormindo e liberando mutex...
");
        pthread_cond_wait(&cond_var, &mutex);
    }

    printf("Consumidor: Acordou e processou os dados com sucesso!
");
    dados_disponiveis = 0;

    pthread_mutex_unlock(&mutex);
    return NULL;
}

void* produtor(void* arg) {
    sleep(2); // Simula preparação dos dados

    pthread_mutex_lock(&mutex);
    dados_disponiveis = 1;
    printf("Produtor: Dados produzidos! Enviando sinal...
");
    
    // Sinaliza uma thread que esteja esperando
    pthread_cond_signal(&cond_var); 

    pthread_mutex_unlock(&mutex);
    return NULL;
}

int main() {
    pthread_t t_cons, t_prod;

    pthread_create(&t_cons, NULL, consumidor, NULL);
    pthread_create(&t_prod, NULL, produtor, NULL);

    pthread_join(t_cons, NULL);
    pthread_join(t_prod, NULL);

    return 0;
}
```

---

## **Tabela Comparativa Geral das Primitivas**

| Primitiva | Comportamento ao Falhar | Conceito de Posse? | Consumo de CPU ao Esperar | Melhor Caso de Uso |
| :--- | :--- | :--- | :--- | :--- |
| **Mutex** | Bloqueia (Dorme) | **SIM** (Apenas o dono libera) | Zero (Gerenciado pelo SO) | Exclusão mútua em regiões críticas gerais. |
| **Semáforo** | Bloqueia (Dorme) | **NÃO** (Qualquer um pode dar `post`) | Zero (Gerenciado pelo SO) | Controle de vazão/limite de recursos N, sinalização. |
| **Spinlock** | Espera Ocupada | **SIM/NÃO** (Depende do SO) | **100% de 1 núcleo** | Seções críticas ultracurtas em kernels/drivers. |
| **Barreira** | Bloqueia (Dorme) | **NÃO** | Zero | Sincronização em fases/passos paralelos. |
| **CondVar** | Bloqueia (Dorme) | **Requer Mutex** | Zero | Espera por eventos ou mudanças lógicas de estado. |

---

## **Exercícios e Questões de Fixação**

1. **Questão de Análise:** Qual a diferença fundamental entre um **Mutex** e um **Semáforo Binário**? Por que libertar um Mutex em uma *thread* diferente daquela que o travou é um erro de programação?
2. **Questão de Engenharia:** Em um servidor com 64 núcleos, uma determinada trava é disputada por dezenas de *threads*, mas o tempo dentro da região crítica é de apenas 5 nanossegundos. Você escolheria **Mutex** ou **Spinlock**? Justifique com base na latência e no custo de troca de contexto.
3. **Desafio de Código:** O que acontece se uma *thread* chamar `pthread_cond_wait()` sem antes ter adquirido o Mutex correspondente? Explique com base nos passos atômicos do `pthread_cond_wait`.

---

## **Conclusão do Módulo 2**
Com esta aula, você domina todas as ferramentas necessárias para sincronizar, proteger e coordenar aplicações concorrentes em memória compartilhada! 

Na próxima aula (**Aula 2.5: Metodologia de Projeto Paralelo — O Modelo de Foster**), aprenderemos a metodologia sistemática em 4 etapas (Particionamento, Comunicação, Aglomeração e Mapeamento) para projetar algoritmos paralelos do zero!
