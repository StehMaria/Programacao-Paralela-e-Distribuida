# Programação Paralela e Distribuída**

#### **Módulo 1: Arquiteturas Paralelas e Fundamentos de Hardware**
*   **O Gargalo de Von Neumann e a Necessidade do Paralelismo** — Limites do desempenho sequencial, *Power Wall*, *Memory Wall* e a transição para múltiplos núcleos.
*   **Hierarquia de Memória e Memórias Cache** — Funcionamento de Caches L1 (L1i/L1d), L2 e L3, linhas de cache e protocolos de coerência (MESI/MOESI).
*   **Organização da Memória Compartilhada** — Sistemas SMP, arquitetura UMA (*Uniform Memory Access*) vs. NUMA (*Non-Uniform Memory Access*), latências e nós NUMA.
*   **Sistemas de Memória Distribuída** — Clusters, nós computacionais independentes, redes e topologias de interconexão.
*   **Taxonomia de Flynn e Modelos de Execução** — Categorias SISD, SIMD, MISD e MIMD. Modelos lógicos de programa: SPMD (*Single Program, Multiple Data*) e MPMD (*Multiple Program, Multiple Data*).

---

#### **Módulo 2: Desenvolvimento de Aplicações Concorrentes e Decomposição**
*   **Processos, Threads e Espaço de Memória** — Diferenças estruturais, custo de criação, isolamento de memória vs. compartilhamento e pilha (*stack*).
*   **Concorrência vs. Paralelismo e Escalonamento** — Gerenciamento pelo Sistema Operacional, troca de contexto (*context switch*) e afinidade de CPU (*thread affinity*).
*   **Desafios da Concorrência** — Condições de corrida (*race conditions*), não-determinismo, *deadlocks*, *livelocks* e *starvation*.
*   **Primitivas de Sincronização e Exclusão Mútua** — Regiões críticas, Mutexes, Semáforos, Spinlocks, Barreiras e Variáveis de Condição.
*   **Metodologia de Projeto Paralelo (Modelo de Foster)** — As quatro etapas: Particionamento, Comunicação, Aglomeração e Mapeamento.
*   **Estratégias de Decomposição** — Decomposição de dados (particionamento de domínio) vs. decomposição funcional (tarefas/divisão e conquista).

---

#### **Módulo 3: Otimização Sequencial, Localidade e Vetorização (SIMD)**
*   **Uso Eficiente da Memória e Padrões de Acesso** — Localidade temporal e espacial, padrões de acesso contíguo (*unit stride*) vs. acesso saltado (*non-unit stride*).
*   **Transformações e Reestruturação de Laços** — Técnicas de *Loop Tiling* / *Blocking*, *Loop Unrolling*, fusão e troca de laços (*loop interchange*).
*   **Diagnóstico e Mitigação de False Sharing** — Causa raiz no nível da linha de cache, uso de variáveis locais (*thread-local*), *padding* e diretivas de alinhamento (`alignas`).
*   **Processamento Vetorial (SIMD)** — Instruções vetoriais (SSE, AVX, AVX-512) e registradores estendidos.
*   **Vetorização Automática e Guiada** — Requisitos para laços vetorizáveis, restrições de dependência de dados, uso de diretivas/pragmas de compilador e opções de otimização (`-O2`, `-O3`, `-ftree-vectorize`).

---

#### **Módulo 4: Análise de Desempenho, Profiling e Modelagem**
*   **Métricas de Desempenho** — Tempo de execução (*wall-clock time* vs. CPU time), *Speedup* relativo/absoluto, Eficiência e Escalabilidade (forte vs. fraca).
*   **Limites Teóricos — Lei de Amdahl** — Fração sequencial crítica, teto de aceleração teórica e retornos decrescentes do aumento de processadores.
*   **Escalabilidade de Problemas — Lei de Gustafson** — Aceleração dimensionada (*scaled speedup*) e o impacto do aumento do tamanho do problema.
*   **Modelos Abstratos de Computação (PRAM)** — Variações do modelo PRAM (EREW, CREW, CRCW) e análise assintótica de algoritmos paralelos.
*   **Profiling e Análise de Gargalos** — Medição de tempo confiável, identificação de gargalos de CPU/memória e uso de ferramentas de análise de desempenho (gprof, perf, VTune, Valgrind/Helgrind).

---

#### **Módulo 5: Programação com Memória Compartilhada (OpenMP e Pthreads)**
*   **Programação de Baixo Nível com POSIX Threads (Pthreads)** — Criação, finalização, passagem de argumentos, gerenciamento de mutexes e barreiras em C.
*   **Introdução ao OpenMP e Modelo Fork-Join** — Diretivas `#pragma omp parallel`, escopo de variáveis (`shared`, `private`, `firstprivate`).
*   **Paralelização de Laços em OpenMP** — A diretiva `#pragma omp for`, cláusula `reduction` para acumulação segura de dados.
*   **Políticas de Escalonamento de Laços** — Cláusulas `schedule(static)`, `schedule(dynamic)`, `schedule(guided)` e balanceamento de carga.
*   **Sincronização em OpenMP** — Diretivas `critical`, `atomic`, `barrier`, `master` e `single`.
*   **Paralelismo de Tarefas no OpenMP** — Diretivas `#pragma omp task`, `#pragma omp taskwait`, grafos de dependência e problemas não estruturados.

---

#### **Módulo 6: Programação com Memória Distribuída (MPI)**
*   **O Modelo de Passagem de Mensagens (MPI)** — Conceitos de *Communicator*, *Rank*, tamanho do grupo (`MPI_COMM_WORLD`), inicialização e finalização.
*   **Comunicação Ponto a Ponto Bloqueante** — Troca de dados com `MPI_Send` e `MPI_Recv`, gerenciamento de *buffers* e riscos de *deadlock*.
*   **Comunicação Ponto a Ponto Não-Bloqueante** — Sobreposição de computação e comunicação usando `MPI_Isend`, `MPI_Irecv` e `MPI_Wait`.
*   **Comunicação Coletiva** — Difusão (`MPI_Bcast`), distribuição (`MPI_Scatter`), coleta (`MPI_Gather`/`MPI_Allgather`).
*   **Operações de Redução Coletiva** — Aplicação de `MPI_Reduce` e `MPI_Allreduce` para agregação de dados distribuídos.
*   **Topologias de Processos e Programação Híbrida** — Topologias cartesianas em MPI, sub-comunicadores e a combinação híbrida MPI + OpenMP (cluster de nós multicore).

---

#### **Módulo 7: Computação Heterogênea e Manycore (GPUs com CUDA e OpenCL)**
*   **Arquitetura de GPUs e Aceleradores** — Comparação CPU vs. GPU, latência vs. vazão (*throughput*), paralelismo massivo.
*   **Modelo de Execução SIMT e Warps** — Execução *Single Instruction, Multiple Threads*, agrupamento em *Warps* e o problema da divergência de *Warps*.
*   **Modelo de Programação CUDA** — Definição de *Kernels*, hierarquia de execução (Grid, Bloco, Thread) e indexação de coordenadas.
*   **Hierarquia de Memória em GPUs** — Memória Global, Memória Compartilhada (*Shared Memory*), Registradores e transferência de dados Host-Device (`cudaMemcpy`).
*   **Otimizações em GPU** — Coalescência de acesso à memória global, prevenção de conflitos de banco (*bank conflicts*) em *shared memory*.
*   **Visão Geral de Padrões Abertos** — Conceitos de OpenCL para portabilidade e diretivas de aceleração de alto nível com OpenACC.

---

#### **Módulo 8: Programação Paralela na Nuvem e Processamento Distribuído**
*   **Computação de Alto Desempenho na Nuvem** — Clusters virtuais, provisionamento elástico, contêineres e IaaS para HPC.
*   **Processamento Massivo de Dados (MapReduce)** — O paradigma Map-Reduce, distribuição de dados, fase de *shuffle* e agregações.
*   **Comunicação Assíncrona e Mensageria** — Arquiteturas baseadas em eventos, filas de mensagens, modelo Pub/Sub e integração de serviços distribuídos.
*   **Tolerância a Falhas e Consistência** — Semânticas de consistência em sistemas distribuídos, replicação de dados, *checkpointing* e recuperação de falhas.

---

#### **Módulo 9: Projeto de Experimentos, Avaliação e Sustentabilidade**
*   **Planejamento de Experimentos Computacionais** — Metodologia experimental, definição de variáveis de controle, cargas de trabalho e reprodutibilidade.
*   **Análise Estatística de Dados** — Coleta de amostras, eliminação de ruídos do SO, intervalos de confiança e análise crítica de métricas.
*   **Apresentação e Relatório de Resultados** — Comunicação técnica objetiva, construção de gráficos de escalabilidade e interpretação de dados.
*   **Aspectos Econômicos e Sustentabilidade (Green HPC)** — Custo de infraestrutura, eficiência energética (*Performance per Watt*), dissipação térmica e preservação do meio ambiente.


