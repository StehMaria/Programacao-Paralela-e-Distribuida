#!/bin/bash

# Cria o arquivo de entrada com o tamanho da matriz
echo "1024 1024 1024" > 1024_input.txt

# Compilação com flag de otimização máxima permitida pelo OpenMP
echo "Compilando mm_omp.c..."
gcc -Wall mm_omp.c -o mm_omp -O3 -fopenmp 

# Arrays de configuração
THREADS=(1 2 4 8 16 32 64 128)
ESTRATEGIAS=("1_Loop_Externo_i" "2_Loop_Intermediario_j" "3_Loop_Interno_k")

echo "Iniciando testes. Os resultados representam o tempo em segundos."

for e in {1..3}; do
    echo "=================================================="
    echo " TESTANDO ESTRATÉGIA: ${ESTRATEGIAS[$e-1]}"
    echo "=================================================="
    
    for t in "${THREADS[@]}"; do
        soma=0
        for run in {1..3}; do
            # O utilitário interno do código fará a medição, ignorando o tempo de rand()
            export OMP_NUM_THREADS=$t
            tempo=$(./mm_omp $e < 1024_input.txt)
            soma=$(echo "$soma + $tempo" | bc -l)
        done
        
        # Calcula a média das 3 execuções
        media=$(echo "scale=4; $soma / 3" | bc -l)
        echo "Threads: $t | Tempo Médio: $media s"
    done
    echo ""
done