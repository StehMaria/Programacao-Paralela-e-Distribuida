/*
** PPD / DC/UFSCar - Helio
** Programa : multiplicacao de matrizes
** Objetivo: paralelizacao com OpenMP
*/

#include <math.h>
#include <stdlib.h> 
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <omp.h> // Incluido para as funções e métricas do OpenMP

float *A, *B, *C;

int main(int argc, char *argv[])
{
   int lin_a,col_a,lin_b,col_b,lin_c,col_c;
   int i,j,k;
   int estrategia = 1; // Variável para controlar qual loop testar via script

   if (argc > 1) estrategia = atoi(argv[1]);

   // Leitura otimizada para evitar warnings do compilador e aceitar redirecionamento
   if (scanf("%d %d %d", &lin_a, &col_a, &col_b) != 3) return 1;
   lin_b = col_a;
   lin_c = lin_a;
   col_c = col_b;

   // Alocacao dinâmica das matrizes, com linhas em sequência 
   A = (float *) malloc ( lin_a * col_a * sizeof(float) );
   B = (float *) malloc ( lin_b * col_b * sizeof(float) );
   C = (float *) malloc ( lin_c * col_c * sizeof(float) );

   // Opa! Vai gerar valores aleatórios em paralelo?
   // Talvez seja o caso de usar rand_r() ao invés de rand()...
   
   // RESPOSTA: Sim, utilizando rand_r() em uma região paralela.
   // Cada thread cria sua própria semente (seed) baseada no seu ID para evitar colisões.
   #pragma omp parallel 
   {
       unsigned int seed = time(NULL) ^ omp_get_thread_num();
       
       #pragma omp for
       for(i=0; i < lin_a * col_a; i++) 
          A[i]=(float)rand_r(&seed) / (float)RAND_MAX; 

       #pragma omp for
       for(i=0; i < lin_b * col_b; i++) 
          B[i]=(float)rand_r(&seed) / (float)RAND_MAX; 
   }

   double start = omp_get_wtime(); // Inicia a medição de tempo

   // calculo da multiplicacao
 
   /* 
   ** RESPOSTAS ÀS PERGUNTAS DO CÓDIGO ORIGINAL:
   ** 1. Qual/quais loop(s) paralelizar? Vale a pena paralelizar todos?
   ** R: Não vale a pena paralelizar todos. O ideal é paralelizar apenas o Loop Externo (i).
   ** 
   ** 2. Qual é o efeito de fazer um parallel for em cada um dos fors abaixo?
   ** R: - Loop Externo (i): Excelente desempenho, grão grosso, boa localidade de cache.
   **    - Loop Intermediário (j): Degradação por Falso Compartilhamento (False Sharing).
   **    - Loop Interno (k): Colapso arquitetural devido ao excesso de overhead e sincronização.
   **
   ** 3. É necessários sincronizar alguma operação, garantindo exclusão mútua?
   ** R: Apenas na estratégia do Loop Interno (k), onde todas as threads tentam somar 
   **    valores parciais na mesma variável. (Usou-se reduction para isso).
   */

   if (estrategia == 1) {
       // --- ESTRATÉGIA 1: LOOP EXTERNO (i) ---
       #pragma omp parallel for private(j, k)
       for(i=0; i < lin_c; i++) {
          for(j=0; j < col_c; j++) {
             // RESPOSTA: pode ser útil usar uma variável auxiliar para os cálculos
             float aux = 0;
             for(k=0; k < col_a; k++) 
                aux += A[i*col_a+k] * B[k*col_b+j];
           
             // RESPOSTA: se usou variável auxiliar, atribui-se seu valor à C[i][j]
             C[i*col_c+j] = aux;
          }
       }
   } 
   else if (estrategia == 2) {
       // --- ESTRATÉGIA 2: LOOP INTERMEDIÁRIO (j) ---
       for(i=0; i < lin_c; i++) {
          #pragma omp parallel for private(k)
          for(j=0; j < col_c; j++) {
             float aux = 0;
             for(k=0; k < col_a; k++) 
                aux += A[i*col_a+k] * B[k*col_b+j];
             C[i*col_c+j] = aux;
          }
       }
   } 
   else if (estrategia == 3) {
       // --- ESTRATÉGIA 3: LOOP INTERNO (k) ---
       for(i=0; i < lin_c; i++) {
          for(j=0; j < col_c; j++) {
             float aux = 0;
             // Necessário garantir exclusão mútua com reduction
             #pragma omp parallel for reduction(+:aux)
             for(k=0; k < col_a; k++) 
                aux += A[i*col_a+k] * B[k*col_b+j];
             C[i*col_c+j] = aux;
          }
       }
   }

   double end = omp_get_wtime();
   printf("%f\n", end - start);

   free(A); free(B); free(C);
   return(0);
}