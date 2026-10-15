# Organização da Memória Compartilhada — SMP, UMA vs. NUMA e Impacto no Código

Compreender como a física da interconexão de processadores e a organização física da RAM moldam o desempenho de aplicações em sistemas de memória compartilhada, diferenciando arquiteturas UMA e NUMA e aprendendo a programar considerando a afinidade de memória e CPU.

---

## 1. Introdução: O Espaço Único de Endereçamento

O processador e as memórias cache lutam diariamente contra a latência da memória RAM. Agora, imagine que seu servidor precisa de muito mais poder computacional. A solução direta é adicionar **múltiplos soquetes de processadores** (*multi-socket*) na mesma placa-mãe.

Quando falamos em **Sistemas de Memória Compartilhada**, estamos nos referindo a um modelo computacional no qual **todos os núcleos de todos os processadores enxergam um único espaço físico e lógico de endereçamento de memória**. 

Se o *Core 0* do *Socket 0* escrever o valor `42` no endereço de memória `0x7FFF0012`, o *Core 63* do *Socket 3* pode ler exatamente o mesmo endereço `0x7FFF0012` e obter o valor `42`.

```
===================================================================
                   ESPAÇO GLOBAL DE ENDEREÇAMENTO (RAM)
   [0x00000000] ....................................... [0xFFFFFFFF]
===================================================================
      ▲               ▲               ▲               ▲
      │               │               │               │
  [Core 0]        [Core 1]        [Core 2]        [Core N]
  (Socket 0)      (Socket 0)      (Socket 1)      (Socket 1)
```

Porém, o fato de o espaço de endereçamento ser **logicamente único** não significa que a velocidade de acesso seja a mesma em todos os cantos do chip. É aqui que nascem as arquiteturas **UMA** e **NUMA**.

---

## 2. Sistemas SMP e a Arquitetura UMA (Uniform Memory Access)

### O que é SMP e UMA?
Historicamente, o modelo mais tradicional de multiprocessamento de memória compartilhada é o **SMP (*Symmetric Multiprocessing*)**. Em um sistema SMP clássico:
1. Todos os processadores são **idênticos** (homogêneos).
2. Todos os processadores compartilham um **barramento central comum** (*Bus*) ou uma chave cruzada (*Crossbar Switch*) para alcançar os módulos de RAM.
3. A arquitetura de acesso é denominada **UMA (*Uniform Memory Access*)**.

> **Definição de UMA:** O tempo de acesso (latência) a qualquer posição da memória RAM é **uniforme** e exatamente igual para qualquer núcleo de processamento do sistema.

### A Analogia do Restaurante e do Buffet Único
Pense na arquitetura UMA como um restaurante onde há uma **única mesa de buffet centralizada** para 4 garçons (núcleos):
* Todos os garçons estão à mesma distância da mesa do buffet.
* Não importa qual garçom vá buscar a refeição (dado), a distância percorrida é sempre a mesma.

*Diagrama do modelo UMA com múltiplos núcleos conectados através de um barramento comum a um único banco de memória RAM*

![Imagem5](./imagens/5.jfif)

### O Gargalo de Escalabilidade do UMA
À medida que a indústria começou a colocar 8, 16 ou 32 núcleos em servidores, o barramento centralizado do UMA virou um **engarrafamento monstruoso**. Todos os núcleos disputando as mesmas linhas de comunicação elétricas criavam colisões e tempos de espera inaceitáveis.

Para permitir que a computação paralela continuasse crescendo, os projetistas de hardware precisavam descentralizar a memória.

---

## 3. A Revolução NUMA (Non-Uniform Memory Access)

### Por que NUMA foi criado?
Para resolver a saturação do barramento central, os engenheiros da Intel, AMD e Sun Microsystems tomaram uma decisão radical: **eliminar o barramento centralizado de memória e integrar os controladores de memória diretamente dentro de cada soquete de processador físico.**

Surgiu assim a arquitetura **NUMA (*Non-Uniform Memory Access*)**.

> **Definição de NUMA:** A memória física RAM é dividida e distribuída fisicamente entre os soquetes de processadores. O tempo de acesso (latência e largura de banda) depende de onde o dado está fisicamente armazenado em relação ao núcleo que fez a requisição.

### A Anatomia de um Nó NUMA (*NUMA Node*)
Em um servidor NUMA moderno (por exemplo, um servidor dual-socket AMD EPYC ou Intel Xeon):
* Um **Nó NUMA (*NUMA Node*)** é o conjunto formado por um grupo de núcleos de CPU + suas caches L1/L2/L3 + seus módulos de memória RAM locais conectados diretamente ao seu controlador de memória.
* Se o *Core 0* do *Socket 0* precisa ler um dado que está nos pentes de RAM conectados ao seu próprio *Socket 0*, dizemos que o acesso é **LOCAL** (baixa latência, altíssima velocidade).
* Se o *Core 0* do *Socket 0* precisa ler um dado que está nos pentes de RAM conectados ao *Socket 1*, o dado precisa viajar através de uma **interconexão de alta velocidade entre soquetes** (como o **Intel UPI/QPI** ou **AMD Infinity Fabric**). Dizemos que esse acesso é **REMOTO** (alta latência, menor largura de banda).

*Estrutura física de 2 Sockets NUMA mostrando memória RAM local e barramento de interconexão ponto-a-ponto*

![Imagem6](./imagens/6.jfif)

### A Analogia do Cozinheiro e dos Armários de Temperos
Imagine dois cozinheiros (Socket 0 e Socket 1) trabalhando em bancadas vizinhas na mesma cozinha:
* **Acesso Local:** O Cozinheiro 0 precisa de pimenta. A pimenta está na prateleira da sua própria bancada (Nó NUMA 0). Ele estica o braço e pega em **1 segundo**.
* **Acesso Remoto:** O Cozinheiro 0 precisa de sal orégano. O sal orégano está na prateleira da bancada do Cozinheiro 1 (Nó NUMA 1). Ele precisa pedir licença, atravessar o corredor, pegar o pote e voltar. Isso leva **3 segundos**.

Ambos os cozinheiros estão na mesma cozinha (mesmo espaço de endereçamento global), mas o custo físico de buscar os ingredientes varia radicalmente conforme a localização!

---

## 4. Coerência de Cache em NUMA: ccNUMA

Como o programador continua enxergando uma única memória RAM contínua, o que acontece se o *Socket 0* e o *Socket 1* tiverem cópias do mesmo dado em suas caches L1/L2/L3 locais e um deles alterar o valor?

A esmagadora maioria dos servidores NUMA modernos são **ccNUMA (*Cache-Coherent NUMA*)**.

### Como funciona o ccNUMA?
O hardware e o controlador de memória mantêm a **coerência de cache transparente via hardware** usando protocolos baseados em diretório ou *snooping* sobre os enlaces de interconexão.
* Quando o *Socket 0* escreve em um endereço de memória que o *Socket 1* também tinha em sua cache, o *Socket 0* envia uma mensagem de invalidação através do enlace UPI/Infinity Fabric.
* A cache do *Socket 1* invalida aquela linha.

> **Resumo Crítico:** O ccNUMA nos entrega a **facilidade de programação** da memória compartilhada, mas nos cobra o **preço do desempenho** se ignorarmos a topologia física da máquina!

---

## 5. O Impacto Prático no Código e no Desempenho

### O Princípio do "First Touch" (Primeiro Toque) no Linux
Esta é a regra mais importante que todo desenvolvedor de código de alto desempenho precisa gravar no cérebro:

> **Regra do First Touch do Kernel Linux:**  
> Quando você chama `malloc()` ou `new` em C/C++, o Sistema Operacional **NÃO aloca a memória física RAM imediatamente**. Ele apenas reserva endereços virtuais.  
> A página de memória física só é alocada e associada a um Nó NUMA **no momento em que a primeira thread escreve ou lê (dá o "primeiro toque") naquela página.**

### O Erro Clássico do Programador Iniciante

Imagine o seguinte fluxo muito comum em programas C/OpenMP:

```c
// 1. A thread principal (Master - rodando no Nó NUMA 0) inicializa a matriz
double *matriz = (double*) malloc(N * sizeof(double));
for (int i = 0; i < N; i++) {
    matriz[i] = 0.0; // <--- PRIMEIRO TOQUE REALIZADO PELA THREAD MASTER NO NÓ 0!
}

// 2. Depois, o programa abre uma região paralela para processar a matriz
#pragma omp parallel for
for (int i = 0; i < N; i++) {
    // As threads do Nó NUMA 1 vão tentar processar partes da matriz...
    // POREM, TODA A MEMORIA FOI FISICAMENTE ALOCADA NO NO NUMA 0!
    matriz[i] = calcula(i); 
}
```

**O que aconteceu aqui?**
Como a thread principal deu o "primeiro toque" inicializando o vetor no loop sequencial, **100% das páginas de memória RAM do vetor foram alocadas fisicamente no Nó NUMA 0**.

Quando a região paralela OpenMP é executada, metade das threads (rodando no Soquete/Nó 1) precisarão buscar e escrever dados atravessando o enlace de interconexão remoto para acessar a RAM do Nó 0. **Desempenho despenca!**

---

## 6. Código de Exemplo: Escrevendo Código NUMA-Aware em C/OpenMP

Vamos ver a forma correta de escrever um código **NUMA-Aware** (consciente da topologia NUMA):

```c
#include <stdio.h>
#include <stdlib.h>
#include <omp.h>

#define N 100000000 // 100 milhões de elementos (~800 MB)

int main() {
    double *vetor = (double*) malloc(N * sizeof(double));

    // =========================================================================
    // FORMA CORRETA (NUMA-AWARE):
    // Garantimos que o PRIMEIRO TOQUE é feito pelas mesmas threads paralelas
    // que farão os cálculos posteriores!
    // =========================================================================
    
    #pragma omp parallel for schedule(static)
    for (int i = 0; i < N; i++) {
        // Cada thread dá o primeiro toque no seu próprio bloco contíguo de dados.
        // As páginas de memória são distribuídas proporcionalmente entre os Nós NUMA!
        vetor[i] = 0.0; 
    }

    double inicio = omp_get_wtime();

    // Loop de processamento pesado com a mesma divisão de trabalho
    #pragma omp parallel for schedule(static)
    for (int i = 0; i < N; i++) {
        vetor[i] = vetor[i] + 3.14159 * i;
    }

    double fim = omp_get_wtime();

    printf("Tempo de Execucao (NUMA-Aware): %f segundos\n", fim - inicio);

    free(vetor);
    return 0;
}
```

### Por que o `schedule(static)` é fundamental aqui?
O `schedule(static)` no OpenMP divide o loop em blocos idênticos e previsíveis e os entrega **sempre para as mesmas threads**. Assim, a Thread 0 (Nó 0) aloca e processa a primeira metade, e a Thread 1 (Nó 1) aloca e processa a segunda metade na sua RAM local.

---

## 7. Ferramentas do Sistema Operacional para Diagnóstico NUMA (Linux)

Como desenvolvedores e engenheiros de HPC, o Linux nos fornece ferramentas essenciais para inspecionar e controlar o comportamento NUMA de um servidor:

### 1. `lscpu` ou `numactl --hardware`
Inspeciona a topologia da máquina, revelando quantos nós NUMA existem e a distância relativa entre eles:

```bash
$ numactl --hardware
available: 2 nodes (0-1)
node 0 cpus: 0 1 2 3 4 5 6 7
node 0 size: 32112 MB
node 1 cpus: 8 9 10 11 12 13 14 15
node 1 size: 32256 MB
node distances:
node   0   1
  0:  10  21
  1:  21  10
```
> *Nota:* A tabela de distâncias mostra que acessar o Nó 0 a partir do Nó 0 custa **10** (referência local). Acessar o Nó 1 a partir do Nó 0 custa **21** (mais que o dobro do tempo!).

### 2. `numastat`
Mostra em tempo real se a sua aplicação está sofrendo de **acessos remotos (misses)**:

```bash
$ numastat
                           node0           node1
numa_hit              1240592811      1198203110
numa_miss               45201923         2103982
numa_foreign             2103982        45201923
interleave_hit             21901           21890
local_node            1240182390      1198102120
other_node              45612344         2204972
```
* **`numa_hit`**: Páginas alocadas no nó pretendido (ótimo).
* **`numa_miss`**: Páginas alocadas em outro nó porque a memória local estava cheia ou por má configuração (alerta de gargalo!).

### 3. Executando Programas com `numactl`
Podemos forçar o sistema operacional a prender um programa a um nó específico sem alterar o código-fonte:

* **Prender o programa aos núcleos e memória do Nó 0 apenas:**
  ```bash
  numactl --cpunodebind=0 --membind=0 ./meu_programa
  ```
* **Distribuir as alocações de memória de forma intercalada (*Interleave*) entre todos os nós:**
  ```bash
  numactl --interleave=all ./meu_programa
  ```
  *(Útil para programas legados onde não é possível modificar o código para seguir a regra do First Touch!)*

---

## 8. Resumo

1. **Memória Compartilhada:** Todo o sistema enxerva um espaço único de endereçamento global, mas a física interna de acesso pode ser homogênea ou heterogênea.
2. **UMA (SMP):** Latência uniforme para qualquer posição de memória. Sofre graves problemas de escalabilidade ao adicionar muitos núcleos.
3. **NUMA:** Memória RAM fisicamente dividida por soquetes/nós. Acessos **locais** são rápidos; acessos **remotos** cruzam enlaces de interconexão (UPI/Infinity Fabric) com penalidade de latência e largura de banda.
4. **ccNUMA:** Garante a coerência de cache entre os soquetes via hardware.
5. **Regra do First Touch:** No Linux, a memória física pertence ao Nó NUMA da thread que realizou a primeira escrita na página.
6. **Boas Práticas:** Inicialize vetores e matrizes em paralelo usando as mesmas threads e a mesma distribuição de laços que realizarão o processamento posterior.

---

