# Aula 6.4: Tipos de Dados Derivados e Estruturas de Dados no MPI

---

## 1. Introdução: Por que precisamos de Tipos Derivados?

Nas aulas anteriores (6.2 e 6.3), trabalhamos com mensagens contendo **tipos primitivos nativos do MPI** (`MPI_INT`, `MPI_DOUBLE`, `MPI_CHAR`) dispostos de forma **contígua na memória** (como um vetor simples em C).

Porém, em simulações científicas e aplicações do mundo real, raramente lidamos apenas com listas simples de números. Frequentemente precisamos transmitir:
1. **Estruturas de Dados Heterogêneas (`struct` em C):** Por exemplo, uma partícula em uma simulação física que possui `id` (inteiro), `posição[3]` (`double`), `velocidade[3]` (`double`) e `massa` (`double`).
2. **Submatrizes e Padrões Não-Contíguos:** Por exemplo, enviar uma **coluna** de uma matriz 2D armazenada em ordem de linhas (*Row-Major Order* em C), onde os elementos de uma mesma coluna estão separados por saltos (*strides*) na memória RAM.
3. **Dados Esparsos ou Irregulares:** Elementos espalhados pela memória sem um padrão fixo de espaçamento.

---

### A Analogia da Mudança Residencial (Cartas vs. Caixas Organizadoras)

Imagine que você precisa enviar 100 objetos variados de uma casa para outra (livros, pratos, canetas):

```
Approach 1: Enviar 100 cartas/encomendas individuais pelo correio
[Objeto 1] -> Correio (Paga taxa de entrega + Latência do frete)
[Objeto 2] -> Correio (Paga taxa de entrega + Latência do frete)
...
[Objeto 100] -> Correio (Paga taxa de entrega + Latência do frete)
Result: Custo absurdo de frete e latência gigante!

Approach 2: Empacotar tudo em uma CAIXA ESTRUTURADA única
[ Objeto 1 | Objeto 2 | ... | Objeto 100 ] -> Correio (1 única taxa de frete!)
Result: Envio eficiente com sobrecarga mínima de rede.
```

* **Sem Tipos Derivados:** Você teria que fazer 100 chamadas de `MPI_Send` (pagando a latência da rede em cada uma) **ou** copiar manualmente todos os elementos dispersos para um vetor temporário (*buffer de cópia*), enviá-lo e depois descompactá-lo no destino.
* **Com Tipos Derivados:** Você instrui a biblioteca MPI sobre a **geometria/layout da memória** (onde cada campo está e qual o salto de bytes entre eles). O MPI cuida do empacotamento, envio e desempacotamento diretamente nos hardwares de rede (como placas InfiniBand com DMA), sem overhead manual de software.

---

## 2. O Ciclo de Vida de um Tipo Derivado no MPI

Criar e utilizar um tipo de dado customizado no MPI exige seguir um ciclo de vida rigoroso em 4 etapas:

```
+-------------------------------------------------------+
|  1. Criação (MPI_Type_contiguous / vector / struct)   |
|     Define a geometria (tipos, blocos, deslocamentos) |
+---------------------------+---------------------------+
                            |
                            v
+-------------------------------------------------------+
|  2. Homologação / Compilação (MPI_Type_commit)        |
|     MPI otimiza e constrói a tabela de mapa de memória|
+---------------------------+---------------------------+
                            |
                            v
+-------------------------------------------------------+
|  3. Uso na Comunicação (MPI_Send, MPI_Recv, Bcast...) |
|     O tipo customizado é passado como 'MPI_Datatype'   |
+---------------------------+---------------------------+
                            |
                            v
+-------------------------------------------------------+
|  4. Liberação de Recursos (MPI_Type_free)              |
|     Desaloca a tabela de memória interna do MPI       |
+-------------------------------------------------------+
```

> ⚠️ **Regra de Ouro:** Você **NUNCA** pode utilizar um tipo de dado derivado em uma chamada de comunicação (`MPI_Send`, `MPI_Recv`, `MPI_Bcast`) sem antes ter executado o `MPI_Type_commit()`. Sem o commit, o tipo é apenas um rascunho e resultará em erro em tempo de execução.

---

## 3. Construtores de Tipos Derivados

O MPI oferece diferentes funções construtoras dependendo do grau de complexidade e padrão de memória dos seus dados.

---

### 3.1 `MPI_Type_contiguous` — Agrupamento Sequencial

É o construtor mais simples. Ele cria um novo tipo de dado que consiste na repetição contígua de um tipo existente.

```c
int MPI_Type_contiguous(int count, MPI_Datatype oldtype, MPI_Datatype *newtype);
```

* **Uso típico:** Definir um bloco de N elementos consecutivos como uma única unidade lógica (ex: um bloco de 3 elementos `double` representando um vetor 3D `Ponto3D`).

---

### 3.2 `MPI_Type_vector` — Padrões com Pulos Regulares (*Strides*)

Permite capturar elementos separados por um espaçamento fixo e uniforme na memória. É perfeito para extrair **colunas de matrizes 2D** armazenadas em ordem de linhas (*Row-Major*).

```c
int MPI_Type_vector(int count, int blocklength, int stride, 
                    MPI_Datatype oldtype, MPI_Datatype *newtype);
```

#### Parâmetros Explicados:
* `count`: Quantidade total de blocos a serem coletados.
* `blocklength`: Número de elementos contíguos em **cada** bloco.
* `stride`: Distância (em número de elementos) entre o início de um bloco e o início do próximo bloco.

#### Visualização de Memória (`MPI_Type_vector`):

Imagine uma matriz $4 	imes 4$ armazenada em memória contígua e queremos extrair a **2ª coluna** (índices marcados com `X`):

```
Matriz 4x4 em Memória Linear (Row-Major):
[ (0,0) | (0,1)X | (0,2) | (0,3) | (1,0) | (1,1)X | (1,2) | (1,3) | (2,0) | (2,1)X | ... ]
          |<------- stride = 4 ------->|
          |blocklength=1|
```

Para extrair a coluna inteira com `MPI_Type_vector`:
* `count = 4` (a matriz tem 4 linhas)
* `blocklength = 1` (cada elemento da coluna é 1 `double`/`int`)
* `stride = 4` (a distância do início de um elemento da coluna até o próximo é de 4 posições na memória)

---

### 3.3 `MPI_Type_indexed` — Padrões Irregulares Esparsos

Quando o espaçamento entre os blocos não é uniforme (por exemplo, ao enviar a parte triangular inferior de uma matriz ou um subgrafo esparso), usamos o `MPI_Type_indexed`.

```c
int MPI_Type_indexed(int count, const int array_of_blocklengths[], 
                     const int array_of_displacements[], 
                     MPI_Datatype oldtype, MPI_Datatype *newtype);
```

* `array_of_blocklengths[]`: Vetor contendo o tamanho de cada bloco individual.
* `array_of_displacements[]`: Vetor contendo o deslocamento (offset em número de elementos) de cada bloco em relação ao início da estrutura.

---

### 3.4 `MPI_Type_create_struct` — Estruturas Heterogêneas (`struct`)

Este é o construtor mais genérico e potente do MPI. Ele permite combinar **diferentes tipos de dados** (`int`, `double`, `char`) com **diferentes tamanhos de bloco** e **deslocamentos em bytes arbitrários**.

```c
int MPI_Type_create_struct(int count, 
                           const int array_of_blocklengths[], 
                           const MPI_Aint array_of_displacements[], 
                           const MPI_Datatype array_of_types[], 
                           MPI_Datatype *newtype);
```

#### O Desafio do *Padding* de Memória e Compiladores

Compiladores em C inserem bytes de preenchimento (*padding*) invisíveis entre os campos de uma `struct` para garantir alinhamento de memória na CPU (ex: alinhar variáveis de 8 bytes em endereços múltiplos de 8).

```
Estrutura em C:
struct Particula {
    int id;           // 4 bytes
    // [4 bytes de padding adicionados pelo compilador!]
    double pos[3];    // 24 bytes (3 * 8)
    double vel[3];    // 24 bytes (3 * 8)
};
```

Por essa razão, **nunca devemos calcular offsets manualmente** somando o tamanho dos tipos (`sizeof(int)`). Devemos utilizar a função **`MPI_Get_address()`** para obter os endereços de memória absolutos e calcular a diferença real em bytes entre cada campo!

---

## 4. Estudo de Caso Prático em C: Transmissão de uma Struct `Particula`

Abaixo apresentamos o código completo e testado em C que cria um tipo derivado MPI para uma estrutura heterogênea `Particula`, transfere um vetor de partículas do Processo 0 para o Processo 1 e valida os dados recebidos.

```c
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <mpi.h>

// Definicao da estrutura C
typedef struct {
    int id;           // ID da particula
    double pos[3];    // Posicao (x, y, z)
    double vel[3];    // Velocidade (vx, vy, vz)
} Particula;

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if (size < 2) {
        if (rank == 0) {
            printf("Erro: Este programa requer pelo menos 2 processos MPI.\n");
        }
        MPI_Finalize();
        return 1;
    }

    // =========================================================================
    // ETAPA 1 & 2: Criacao e Commit do Tipo Derivado MPI para 'Particula'
    // =========================================================================
    MPI_Datatype MPI_PARTICULA;
    
    // 1. Especificar a quantidade de campos da struct
    int count = 3;

    // 2. Tamanho de cada bloco/campo (número de elementos por campo)
    int blocklengths[3] = {1, 3, 3}; // 1 int, 3 doubles, 3 doubles

    // 3. Tipos de dados MPI de cada campo
    MPI_Datatype types[3] = {MPI_INT, MPI_DOUBLE, MPI_DOUBLE};

    // 4. Calcular os deslocamentos (offsets) reais em bytes com MPI_Get_address
    MPI_Aint displacements[3];
    Particula p_exemplo;
    MPI_Aint base_addr, addr_id, addr_pos, addr_vel;

    MPI_Get_address(&p_exemplo, &base_addr);
    MPI_Get_address(&p_exemplo.id, &addr_id);
    MPI_Get_address(&p_exemplo.pos, &addr_pos);
    MPI_Get_address(&p_exemplo.vel, &addr_vel);

    displacements[0] = addr_id - base_addr;   // Offset do campo 'id'
    displacements[1] = addr_pos - base_addr;  // Offset do campo 'pos'
    displacements[2] = addr_vel - base_addr;  // Offset do campo 'vel'

    // 5. Construir o tipo derivado
    MPI_Type_create_struct(count, blocklengths, displacements, types, &MPI_PARTICULA);

    // 6. HOMOLOGAR / COMMIT DO TIPO (Obrigatório!)
    MPI_Type_commit(&MPI_PARTICULA);

    // =========================================================================
    // ETAPA 3: Uso do Tipo Derivado na Comunicacao Ponto a Ponto
    // =========================================================================
    if (rank == 0) {
        // Processo 0 inicializa e envia dados de uma particula
        Particula p_envio;
        p_envio.id = 1042;
        p_envio.pos[0] = 12.5; p_envio.pos[1] = -3.4; p_envio.pos[2] = 0.8;
        p_envio.vel[0] = 1.0;  p_envio.vel[1] = 0.5;  p_envio.vel[2] = -2.1;

        printf("[Rank 0] Enviando Particula ID %d (Pos: %.1f, %.1f, %.1f)...\n",
               p_envio.id, p_envio.pos[0], p_envio.pos[1], p_envio.pos[2]);

        // Envia 1 elemento do tipo customizado 'MPI_PARTICULA' para o Rank 1
        MPI_Send(&p_envio, 1, MPI_PARTICULA, 1, 99, MPI_COMM_WORLD);

    } else if (rank == 1) {
        // Processo 1 recebe os dados diretamente na struct local
        Particula p_recebida;

        MPI_Recv(&p_recebida, 1, MPI_PARTICULA, 0, 99, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        printf("[Rank 1] Particula Recebida com Sucesso!\n");
        printf("         ID: %d\n", p_recebida.id);
        printf("         Posicao:    [%.1f, %.1f, %.1f]\n", 
               p_recebida.pos[0], p_recebida.pos[1], p_recebida.pos[2]);
        printf("         Velocidade: [%.1f, %.1f, %.1f]\n", 
               p_recebida.vel[0], p_recebida.vel[1], p_recebida.vel[2]);
    }

    // =========================================================================
    // ETAPA 4: Liberacao do Tipo Derivado
    // =========================================================================
    MPI_Type_free(&MPI_PARTICULA);

    MPI_Finalize();
    return 0;
}
```

---

## 5. Empacotamento Manual com `MPI_Pack` e `MPI_Unpack`

Em alguns cenários dinâmicos complexos (como árvores, grafos flutuantes ou estruturas onde os tipos de dados variam a cada mensagem), definir um `MPI_Datatype` estático pode ser inviável.

Nesses casos, o MPI oferece as funções de **serialização manual**:
* **`MPI_Pack()`:** Copia elementos de diferentes regiões de memória para um único buffer contíguo de bytes (`MPI_PACKED`).
* **`MPI_Unpack()`:** Extrai elementos sequencialmente do buffer de bytes recebido para variáveis e estruturas locais.

```c
// Processo Remetente:
int position = 0; // Offset no buffer de bytes
char buffer[1024];

MPI_Pack(&id, 1, MPI_INT, buffer, 1024, &position, MPI_COMM_WORLD);
MPI_Pack(vetor_double, 10, MPI_DOUBLE, buffer, 1024, &position, MPI_COMM_WORLD);

// Envia o buffer compilado de bytes (tam em 'position')
MPI_Send(buffer, position, MPI_PACKED, 1, 0, MPI_COMM_WORLD);
```

> 💡 **Comparação:** `MPI_Type_create_struct` tem desempenho superior pois o MPI otimiza o deslocamento diretamente em nível de hardware/rede. `MPI_Pack/Unpack` adiciona overhead de cópia extra em memória na CPU, devendo ser reservado para estruturas altamente dinâmicas.

---

## 6. Tabela Resumo dos Construtores de Tipos MPI

| Construtor MPI | Padrão de Memória | Uso Típico |
| :--- | :--- | :--- |
| **`MPI_Type_contiguous`** | Blocos contíguos do mesmo tipo | Vetores 3D, arrays de tamanho fixo. |
| **`MPI_Type_vector`** | Blocos do mesmo tipo separados por saltos (*strides*) constantes | Colunas ou submatrizes 2D/3D em matrizes contíguas. |
| **`MPI_Type_indexed`** | Blocos do mesmo tipo com tamanhos e deslocamentos arbitrários | Matrizes esparsas, polígonos, subgrafos. |
| **`MPI_Type_create_struct`** | Blocos de **tipos diferentes** com deslocamentos arbitrários em bytes | Estruturas C (`struct`), registros de dados heterogêneos. |
| **`MPI_Pack / MPI_Unpack`** | Serialização manual em buffer de bytes contíguo | Dados com tamanhos e formatos dinâmicos em tempo de execução. |

---

## 7. Exercícios de Fixação

### Questão 1
Por que é indispensável utilizar a função `MPI_Get_address()` ao calcular os deslocamentos dos campos de uma `struct` em C para `MPI_Type_create_struct`, em vez de simplesmente somar `sizeof(tipo)`?
* **A)** Porque `sizeof` não funciona com variáveis do tipo `double`.
* **B)** Porque o compilador em C pode inserir bytes de preenchimento (*padding*) invisíveis entre os campos da `struct` para alinhamento de memória da CPU.
* **C)** Porque a rede InfiniBand só aceita endereços de memória de 64 bits.
* **D)** Porque `MPI_Type_create_struct` exige obrigatoriamente que todos os campos tenham o mesmo tipo de dado.

### Questão 2
Qual é o papel da função `MPI_Type_commit()` na criação de tipos derivados?
* **A)** Ela envia a definição da estrutura para todos os outros nós do cluster via broadcast.
* **B)** Ela aloca a memória necessária para armazenar as variáveis da struct.
* **C)** Ela homologa e otimiza a representação interna da tabela do mapa de memória no MPI, tornando o tipo derivado pronto para uso em chamadas de comunicação.
* **D)** Ela libera os recursos de memória utilizados pelo construtor do tipo derivado.

---

### Gabarito Comentado

* **Questão 1:** **Resposta Correta: B**. Compiladores C alinham variáveis em múltiplos de 4 ou 8 bytes por razões de eficiência do hardware, criando lacunas de *padding*. `MPI_Get_address()` captura os endereços físicos reais em RAM, garantindo a medição exata do deslocamento em bytes.
* **Questão 2:** **Resposta Correta: C**. Sem o `MPI_Type_commit()`, o MPI não compila a tabela interna de offsets, e qualquer tentativa de usar o novo `MPI_Datatype` em chamadas como `MPI_Send` ou `MPI_Recv` resultará em erro fatal de execução.
