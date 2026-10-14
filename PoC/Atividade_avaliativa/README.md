# Análise de Escalabilidade: Multiplicação de Matrizes com OpenMP 🚀

Este repositório contém o estudo de caso prático sobre os limites do paralelismo utilizando a biblioteca **OpenMP** na linguagem C. O projeto foi desenvolvido como atividade avaliativa para a disciplina de Programação Paralela e Distribuída (PPD).

## 📌 Objetivo do Projeto

O objetivo principal deste experimento não é apenas paralelizar o código, mas **investigar o impacto arquitetural** da aplicação de diretivas de paralelismo em diferentes níveis de profundidade de loops aninhados (externo, intermediário e interno) na multiplicação de matrizes densas ($1024 \times 1024$).

Durante o estudo, foram explorados conceitos avançados de Arquitetura de Computadores e Sistemas Operacionais, tais como:
- Granularidade (Grão Grosso vs. Grão Fino)
- Localidade Espacial de Cache
- Falso Compartilhamento (*False Sharing*)
- Condição de Corrida (*Race Condition*) e Exclusão Mútua (`reduction`)
- Paradoxo da Contenção e *Overhead* de Troca de Contexto (*Context Switching*)

## 🗂️ Estrutura de Arquivos

- `mm_omp.c`: Código-fonte comentado em C contendo a implementação da multiplicação e as três estratégias de paralelização. Inclui também a correção de *thread-safety* utilizando `rand_r()` no lugar de `rand()`.
- `run.sh`: Script Bash para automação de testes locais (Linux/WSL), executando o código com 1 até 128 threads e calculando a média de tempo.

## 🚀 Como Executar

### Ambiente Local (Linux / WSL)
Para rodar a bateria de testes no seu próprio computador:

1. Clone o repositório e navegue até a pasta:
   
2. Dê permissão de execução e corrija possíveis quebras de linha do Windows:
   ```bash
   chmod +x run.sh
   sed -i 's/\r$//' run.sh
   ```
3. Execute o script:
   ```bash
   ./run.sh
   ```
