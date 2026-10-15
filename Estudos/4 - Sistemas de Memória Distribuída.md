# Sistemas de Memória Distribuída (Clusters, Redes e Topologias)

### 1. Introdução: A Era do Dimensionamento Horizontal (*Scale-Out*)

Os engenheiros espremeram múltiplos núcleos (*cores*) e múltiplos soquetes dentro de um único servidor (sistemas SMP e NUMA de memória compartilhada). Mas o que acontece quando precisamos resolver um problema tão gigantesco que **nenhum servidor no mundo**, por mais caro que seja, tem memória RAM ou núcleos suficientes?

A resposta é o **dimensionamento horizontal (*scale-out*)**: em vez de tentar construir um único "super-servidor" monstruoso e caríssimo, nós conectamos dezenas, centenas ou milhares de computadores individuais por meio de uma rede de altíssima velocidade. 

---

### 2. Analogia: A Rede de Restaurantes Independente

Imagine a diferença entre uma grande cozinha corporativa (Memória Compartilhada) e uma rede de franquias de restaurantes (Memória Distribuída):

* **Memória Compartilhada (SMP/NUMA):** Vários chefs trabalham dentro da mesma cozinha. Todos conseguem pegar ingredientes na mesma geladeira central (RAM). Se um chef tempera um molho, o outro enxerga imediatamente. É rápido, mas se a cozinha ficar lotada demais, os chefs começam a trombá-los nos corredores (gargalo de barramento/coerência).
* **Memória Distribuída (Cluster):** Cada chef trabalha em seu próprio restaurante em um bairro diferente. Cada um tem sua própria geladeira privada, seu próprio fogão e seus próprios ajudantes. Eles **não conseguem** esticar o braço e pegar o sal da geladeira do outro. Se o Restaurante A precisa enviar uma receita ou ingrediente para o Restaurante B, ele precisa obrigatoriamente **empacotar a informação e mandar um motoboy via rua (rede)**.

---

### 3. Anatomia de um Cluster de Computadores

Um **Cluster** é um conjunto de computadores autônomos (chamados de **nós** ou *nodes*) interconectados por uma rede local de alto desempenho, trabalhando juntos como se fossem um único recurso computacional integrado.

*Diagrama de arquitetura de um Cluster HPC mostrando o Head Node conectado via rede InfiniBand aos Compute Nodes e ao armazenamento compartilhado NFS/Lustre*

![Imagem7](./imagens/7.jfif)

### Componentes Chave de um Cluster:

1. **Nó Mestre / Cabeça (*Head Node / Login Node*):** É a porta de entrada. É onde o programador faz login via SSH, compila os programas e submete os trabalhos para a fila de execução (*Job Scheduler* como Slurm ou PBS).
2. **Nós de Processamento (*Compute Nodes*):** As "operárias" do cluster. Geralmente são servidores de rack (*blade servers*) sem monitor ou teclado, dedicados exclusivamente a executar as tarefas pesadas.
3. **Rede de Interconexão de Alta Velocidade:** O "sangue" do cluster. Interconecta as placas de rede dos nós com baixíssima latência e altíssima largura de banda.
4. **Sistema de Arquivos Distribuído (Storage / SAN / Lustre / NFS):** Como a RAM de cada nó é privada, os arquivos de entrada e saída ficam em um armazenamento em rede centralizado, permitindo que qualquer nó leia ou grave arquivos de dados.

---

### 4. Métrica e Desempenho de Redes: As Duas Forças do Conectivo

Quando trabalhamos em memória distribuída, o custo do transporte de dados via rede passa a ser o principal gargalo. A performance da rede é medida por duas métricas fundamentais:

### 4.1. Latência ($\tau$)
É o tempo decorrido para enviar uma mensagem de tamanho **zero** de um nó A para um nó B. Inclui o tempo de preparação do pacote no sistema operacional, travessia da placa de rede, comutação nos *switches* e chegada ao destino.
* **Rede Ethernet Comum (1Gbps):** Latência alta (~10 a 50 microssegundos).
* **InfiniBand / RDMA:** Latência ultrabaixa (~0.5 a 2 microssegundos).

### 4.2. Largura de Banda ou Vazão ($\beta$ - *Bandwidth*)
É a taxa máxima de transferência de dados contínua que a rede suporta, geralmente expressa em Gigabits por segundo (Gbps) ou Gigabytes por segundo (GB/s).
* **Fórmula do Tempo de Comunicação de uma Mensagem de $L$ bytes:**
$$\text{Tempo de Transmissão} = \text{Latência} + \frac{\text{Tamanho da Mensagem (bytes)}}{\text{Largura de Banda}}$$

> 💡 **Lição Prática de Projeto:** 
> Se o seu algoritmo envia milhares de mensagens **muito pequenas** (poucos bytes), a **latência** vai matar o desempenho do seu código. É melhor agrupar várias mensagens pequenas em um único pacote grande antes de enviar (*Message Aggregation*) para amortizar o custo da latência!

---

### 5. Topologias de Interconexão de Redes

Como devemos ligar os cabos entre 100, 1.000 ou 10.000 nós de um supercomputador? Ligar todo mundo em todo mundo (*Fully Connected*) exige um número astronômico de cabos e portas de rede ($N(N-1)/2$). Por isso, os engenheiros utilizam **topologias de rede**.

Para avaliar uma topologia, analisamos:
* **Diâmetro da Rede:** A maior distância (em número de saltos / *hops*) entre quaisquer dois nós. Quanto menor, melhor.
* **Grau dos Nós (*Degree*):** Número de conexões/portas de rede físicas saindo de cada nó.
* **Largura de Banda de Bisseção (*Bisection Bandwidth*):** Se dividirmos a rede ao meio no seu ponto mais fraco, qual é a taxa de transferência máxima entre as duas metades? Quanto maior, melhor.

Abaixo, veja as principais topologias históricas e modernas:

---

### 5.1. Barramento / Bus
```
 [Nó 1]     [Nó 2]     [Nó 3]     [Nó 4]
   |          |          |          |
===+==========+==========+==========+=== (Barramento Compartilhado)
```
* **Descrição:** Todos os nós compartilham um único cabo/meio físico.
* **Prós:** Simples e barato.
* **Contras:** Apenas dois nós podem conversar por vez. Não escala para mais de uma dezena de máquinas.

---

### 5.2. Anel (*Ring*)
```
  [Nó 1] ---- [Nó 2]
    |           |
  [Nó 4] ---- [Nó 3]
```
* **Diâmetro:** $N/2$.
* **Grau:** Fixado em 2 conexões por nó.
* **Uso:** Usado internamente em anéis de comunicação dentro de alguns chips multicore e redes locais simples.

---

### 5.3. Malha 2D (*2D Mesh / Grid*)

*Ilustração de uma grade/malha 2D e 3D mostrando nós dispostos em coordenadas X, Y, Z*

![Imagem8](./imagens/8.jfif)


* **Diâmetro:** $2 \times (\sqrt{N} - 1)$.
* **Grau:** 4 (no plano 2D) ou 6 (no cubo 3D).
* **Uso:** Muito popular em supercomputadores para simulações físicas de fluidos e clima onde cada pedaço da malha conversa apenas com seus vizinhos geográficos imediatos.

---

### 5.4. Hipercubo (*Hypercube*)

*Progressão geométrica do Hipercubo de 0D a 4D com nós numerados em código binário*

![Imagem9](./imagens/9.jfif)

* **Estrutura:** Um hipercubo de dimensão $d$ possui $N = 2^d$ nós. Dois nós são conectados se seus endereços binários diferirem em apenas 1 bit.
* **Diâmetro:** $\log_2(N)$ (MUITO PEQUENO!).
* **Grau:** $\log_2(N)$ (cresce com o tamanho do cluster).
* **Vantagem:** Diâmetro pequeníssimo e altíssima taxa de bisseção.

---

### 5.5. Árvore Gorda (*Fat-Tree*) — O Padrão dos Data Centers e HPC

*Esquema de uma rede Fat-Tree mostrando cabos mais largos/múltiplos enlaces subindo em direção aos switches do topo*

![Imagem10](./imagens/10.jfif)

* **O Conceito Genial:** Em uma árvore comum, os links do topo viram um gargalo de garrafa enorme. Na **Fat-Tree**, à medida que subimos na hierarquia em direção à raiz, a **largura de banda dos cabos/links aumenta** (ou o número de cabos paralelos é multiplicado).
* **Por que é o padrão atual?** Garante que **qualquer nó possa se comunicar com qualquer outro nó em velocidade total sem bloqueios** (*non-blocking bisection bandwidth*). Usado na grande maioria dos clusters moderníssimos com InfiniBand.

---

### 6. Exemplo Prático em C: O Modelo de Passagem de Mensagens (Conceitual via Sockets)

Em memória compartilhada (OpenMP), duas threads trocam dados alterando a mesma variável global na RAM. Em memória distribuída, um nó **deve enviar explicitamente** (`send`) e o outro **deve receber explicitamente** (`receive`).

Abaixo temos um exemplo em C puro ilustrando a diferença fundamental da comunicação por sockets (que serve de base conceitual para o **MPI - Message Passing Interface** que estudaremos em detalhes no Módulo 6):


```c
// ============================================================================
// Programa Conceitual: Envio de Mensagem Explicita entre No A e No B
// ============================================================================
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Estrutura do Pacote de Dados de Mensagem Distribuida
typedef struct {
    int id_origem;
    int id_destino;
    double carga_util[4]; // Dados processados
} PacoteMensagem;

// Simulacao do No Remetente (Node 0)
void executar_no_remetente() {
    PacoteMensagem msg;
    msg.id_origem = 0;
    msg.id_destino = 1;
    msg.carga_util[0] = 3.14159;
    msg.carga_util[1] = 2.71828;

    printf("[NO 0] Preparando dados na RAM Privada do No 0...\n");
    printf("[NO 0] ENVIANDO mensagem para a Placa de Rede (InfiniBand/Ethernet)...\n");
    
    // Na pratica usariamos: MPI_Send(&msg, sizeof(msg), ..., 1, ...);
    // O dado sai da RAM do No 0, atravessa o cabo de rede e chega no No 1.
}

// Simulacao do No Destinatario (Node 1)
void executar_no_destinatario() {
    PacoteMensagem msg_recebida;

    printf("[NO 1] AGUARDANDO chegada de dados na placa de rede...\n");
    
    // Na pratica usariamos: MPI_Recv(&msg_recebida, sizeof(msg_recebida), ..., 0, ...);
    // Simulação de recebimento:
    msg_recebida.id_origem = 0;
    msg_recebida.carga_util[0] = 3.14159;

    printf("[NO 1] Mensagem recebida da rede com sucesso!\n");
    printf("[NO 1] Dado lido na RAM Privada do No 1: PI = %.5f\n", msg_recebida.carga_util[0]);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Uso: %s [no0 | no1]\n", argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "no0") == 0) {
        executar_no_remetente();
    } else if (strcmp(argv[1], "no1") == 0) {
        executar_no_destinatario();
    } else {
        printf("No desconhecido.\n");
    }

    return 0;
}
```

---

### 7. Tabela Comparativa: Memória Compartilhada vs. Memória Distribuída

| Critério | Memória Compartilhada (SMP / NUMA) | Memória Distribuída (Clusters) |
| :--- | :--- | :--- |
| **Escopo de Endereço** | Endereçamento global único (RAM comum). | Múltiplos espaços de RAM privados isolados. |
| **Comunicação** | Leitura/Escrita direta em variáveis globais. | Envio e recebimento explícito de mensagens (`Send`/`Recv`). |
| **Escalabilidade** | Limitada (até dezenas/centenas de núcleos no mesmo server). | Gigantesca (milhares de nós / milhões de núcleos). |
| **Custo de Hardware** | Servidores NUMA grandes são proporcionalmente caros. | Nós individuais de mercado (*commodity*) interligados por rede. |
| **Mecanismo de Controle** | Primitivas de sincronização (*Mutex*, *Lock*, *Barrier*). | Protocolos de rede / Biblioteca MPI. |
| **Principal Gargalo** | Disputa no barramento e coerência de cache (*False Sharing*). | Latência de rede e largura de banda. |

---

### 8. Resumo

1. **Memória Distribuída** é a única forma de escalar o processamento para milhares de máquinas conectadas por rede (Clusters).
2. Não existe memória RAM compartilhada entre os nós: **se um nó não enviar o dado via rede, o outro nó jamais verá essa informação**.
3. A performance de um cluster depende criticamente da **latência** e da **largura de banda** da rede de interconexão.
4. Redes modernas de supercomputadores usam a topologia **Fat-Tree** com tecnologias como **InfiniBand / RDMA** para garantir comunicação rápida e sem bloqueios.
5. O paradigma de programação dominante para clusters é o **MPI (Message Passing Interface)**, baseado no modelo SPMD.

---
