/**João Victor Schindler Leite 
 * ================================================================
 * main.c
 * ---------------------------------------------------------------
 * Módulo principal do programa.
 *
 * Este arquivo implementa a execução principal do jogo Campo Minado,
 * cuja lógica de resolução automática utiliza um resolvedor SAT externo
 * (CaDiCaL) para deduzir células seguras com base no estado atual
 * do tabuleiro e nas restrições codificadas em CNF (Clausal Normal Form).
 *
 * O programa lê dois arquivos de entrada:
 * 1. initial-grid-path.in — estado inicial do jogador (células visíveis)
 * 2. ground-truth-path.in — configuração real das bombas
 *
 * A cada iteração, o programa deverá:
 * - Gerar um arquivo CNF descrevendo as restrições do tabuleiro;
 * - Invocar o resolvedor CaDiCaL para inferir se uma célula contém bomba;
 * - Revela automaticamente células seguras;
 * - Utiliza um “oráculo” caso não haja células seguras dedutíveis.
 *
 * Dependências:
 * - utils.h        (funções auxiliares)
 * - minesweeper.h  (estrutura e funções do jogo)
 * - CaDiCaL        (resolvedor SAT externo, chamado via popen())
 *
 * Compilação:
 * Este arquivo é compilado automaticamente via Makefile.
 * ================================================================
 */

#include <stdio.h>
#include <stdlib.h>

#include "minesweeper.h"
#include "parser.h"
#include "utils.h"

// --- FUNÇÕES AUXILIARES DE LÓGICA ---

/***Explicação detalhada da função gerar_combinacoes(),que vem a seguir:
 * Gera as cláusulas de proibição e retorna QUANTAS foram geradas.
 * Se f for NULL, apenas conta sem escrever.
 */
/**1.Utilizei 2^n para gerar a tabela-verdade de cada vizinho de uma célula,
* já que cada vizinho pode ser bomba ou não ser,há 2 possibilidades por vizinho.
* O algoritmo percorre todas as combinações possíveis de bombas entre os vizinhos 
* e gera cláusulas de proibição para aqueles casos em que o número da "dica"(k)
* não coincide com a contagem de bombas na combinação analisada.
*2.Usei bitshifting porque queremos tratar o número i como uma sequência de binários,
*Para verificar todas as disposições possíveis de bombas entre os vizinhos, utilizamos
*uma abordagem de "força bruta" manipulando bits.A variável iteradora i atua como um vetor de "estados" binários,
* Cada bit em i representa se um vizinho específico é uma bomba (1) ou não (0).
* Se o j-ésimo bit de i for 1, incrementamos o contador de bombas atuais.Fazemos isso
* para um inteiro percorre todas as possibilidades sem a necessidade de alocar mais memória
*A operação (i >> j) & 1 extrai o estado (0 ou 1) de apenas um vizinho específico,via operações bit a bit.
 * 3. Tradução para CNF (Lei de De Morgan):
 * Quando encontramos uma combinação de vizinhos cuja soma de bombas está ERRADA
 * (diferente de k), precisamos criar uma cláusula que PROÍBA essa combinação específica,isto é,
 * garantindo que o resolvedor SAT não a considere uma solução válida.
 * A lógica segue a Lei de De Morgan:
 * 1. Vamos supor que a combinação proibida é: (A é Bomba) E (B é Bomba).
 * 2. Para proibir isso, exigimos logicamente o OPOSTO: NÃO [ (A é Bomba) E (B é Bomba) ].
 * 3. Isso se traduz na disjunção: (A NÃO é Bomba) OU (B NÃO é Bomba).
 *
 * No formato CNF:
 * - Se na "combinação ruim" a célula era BOMBA (1), escrevemos a variável NEGATIVA (-var).
 * - Se na "combinação ruim" a célula era SEGURA (0), escrevemos a variável POSITIVA (var).
 *
 * NOTE:
 * Escrever "-var" nesse caso não significa dizer que aquela célula é garantidamente segura.
 * Significa apenas que, para a cláusula em questão ser satisfeita (e "driblarmos" o cenário
 * da combinação errada), aquela célula precisaria mudar de estado.
 *
 * É a conjunção (AND) de todas essas cláusulas de negação que, juntas, "formam"
 * a solução e delimitam a quantidade exata de bombas permitidas, eliminando
 * tudo o que for excesso ou falta.
 */
int gerar_combinacoes(FILE* f, int* vizinhos, int n_vizinhos, int k) {//Passo 1
    int count = 0;//contador de celulas geradas
    int limite = 1 << n_vizinhos; // 2^n combinações possíveis

    for (int i = 0; i < limite; i++) {//percorre a tabela-verdade
        int bombas_atuais = 0;//contador "real" de bombas na combinação atual

        for (int j = 0; j < n_vizinhos; j++) {//Passo 2
            if ((i >> j) & 1) bombas_atuais++;
        }

        // Se a contagem está certa, a combinação é válida. Pula.
        if (bombas_atuais == k) continue;

        // Se chegou aqui, gerou uma regra de proibição.
        count++;

        // Se a combinação é inválida, escreve a cláusula de proibição
        if (f != NULL) {
            for (int j = 0; j < n_vizinhos; j++) {
                int var = vizinhos[j];
                int eh_bomba = (i >> j) & 1;//Extraindo se o vizinho j é bomba na combinação atual
                
               /* -Se na combinação "ruim" o vizinho era BOMBA, escrevemos a variável NEGATIVA (-var).
               * - Se na combinação "ruim" o vizinho era SEGURO, escrevemos a variável POSITIVA (var).
               *Lógica De Morgan: Inverte o que vê.
               /*/
                if (eh_bomba) fprintf(f, "-%d ", var);
                else          fprintf(f, "%d ", var);
            }
            fprintf(f, "0\n");
        }
    }
    return count;
}

/**
 * Percorre todo o tabuleiro, identifica as restrições (células abertas e dicas numéricas)
 * e gera as regras lógicas correspondentes no formato DIMACS CNF(para o CaDiCaL).
 * * Retorna o número total de cláusulas geradas.
 * Se o ponteiro 'f' for NULL, a função apenas conta as cláusulas sem escrever no arquivo.
 */
int gerar_regras(FILE* f, minesweeper_t* ms) {
    int total_clausulas = 0;
    int rows = ms->rows;
    int cols = ms->cols;

    // Itera sobre cada célula do tabuleiro para aplicar as regras do jogo
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            // Fatos Conhecidos (Células Reveladas)
            // Se a célula já está aberta, sabemos com certeza que ela é segura.
            if (ms->revealed_grid[i][j]) {
                total_clausulas++;
                
                if (f != NULL) {
                    int var = coords_to_var(cols, i, j);
                    
                    // Por que negativo? (-var)
                    // No meu modelo, a variável positiva significa "há bomba".
                    // Como a célula está revelada, ela não tem bomba,trivialmente.
                    // Logo, escrevemos a negação (-var) para afirmar: "É garantidamente falso que existe uma bomba aqui".
                    // Isso é uma "Cláusula Unitária",ou seja, uma fórmula que se refere apenas a uma variável.
                    fprintf(f, "-%d 0\n", var); 
                }
            }

            // Dicas Numéricas (Restrições de Vizinhança)
            // Se a célula revelada exibe um número 'k', isso traduz-se em uma regra lógica:
            // "Dentre os vizinhos desta célula(usando o conceito do enunciado), exatamente 'k' são bombas".
            if (ms->revealed_grid[i][j] && ms->player_grid[i][j] >= 0) {
                int k = ms->player_grid[i][j];// número de bombas indicadas pela dica
                int vizinhos[8]; // Array para guardar os IDs das variáveis vizinhas
                int n_count = 0;

                // Varredura dos adjacentes (matriz 3x3 centrada em i,j)
                for (int di = -1; di <= 1; di++) {//deslocamento relativo linha
                    for (int dj = -1; dj <= 1; dj++) {//deslocamento relativo coluna
                        // Pula a própria célula central (deslocamento 0,0)
                        if (di == 0 && dj == 0) continue;
                        
                        // Calcula coordenada absoluta do vizinho
                        int ni = i + di;//Em "formato de matriz"
                        int nj = j + dj;

                        // Verifica se o vizinho existe (está dentro dos limites da matriz)
                        if (ms_in_bounds(ni, nj, rows, cols)) {
                            // Converte a coordenada (ni, nj) para o ID único da variável SAT
                            vizinhos[n_count++] = coords_to_var(cols, ni, nj);
                        }
                    }
                }
                
                // Chama a função auxiliar que gera todas as combinações matemáticas possíveis
                // para esses vizinhos e proíbe aquelas cuja soma de bombas seja diferente de 'k'(inválidas).
                total_clausulas += gerar_combinacoes(f, vizinhos, n_count, k);
            }
        }
    }
    return total_clausulas;
}

/**
 * @brief Procura uma célula ainda não revelada e tenta determinar,
 * via resolução SAT, se há uma bomba naquela posição.
 *
 * A função deve gerar dinamicamente um arquivo CNF descrevendo as restrições
 * do tabuleiro atual, chama o resolvedor CaDiCaL e interpreta o retorno
 * para decidir uma célula que deve ser revelada a seguir.
 *
 * @param ms Ponteiro para a estrutura do jogo (estado atual do tabuleiro).
 * @param r  Ponteiro para armazenar a linha escolhida.
 * @param c  Ponteiro para armazenar a coluna escolhida.
 */

 /* ESTRATÉGIA DE LÓGICA PROPOSICIONAL (PROVA POR CONTRADIÇÃO):
 * Esta função utiliza o conceito de redução ao absurdo para encontrar segurança.
 * O solver CaDiCaL não "sabe" onde não tem bomba, ele apenas verifica a satisfatibilidade
 * de uma dada fórmula no formato CNF.
 * Para cada célula ainda não revelada (candidata), executamos o seguinte algoritmo lógico:   
 * 1. Definimos uma Hipótese (H): "Existe uma bomba/mina na coordenada (i, j)".
 * Em CNF, isso é escrito como uma cláusula unitária positiva: "VAR 0"(sinal positivo,pois 1=bomba).
 *
 * 2. Raciocinamos sobre aquilo que já sabemos(K): Todas as regras que vem das dicas numéricas
 * do tabuleiro (ex: "aqui tem 1 bomba", "ali tem 2 bombas").
 *
 * 3. Perguntamos ao Solucionador: A fórmula (H AND K) é satisfatível?
 * INTERPRETAÇÃO DO RESULTADO:
 * - Se SAT (Código 10, de acordo com o manual do solucionador): Existe pelo menos um cenário onde é possível ter uma mina ali
 * sem quebrar as regras. Não podemos concluir nada, ou seja, não há garantia que a célula seja segura.
 *
 * - Se UNSAT (Código 20): A hipótese H(que foi assumida como verdadeira) entra em contradição direta com o K.
 * É logicamente impossível existir uma mina nessa célula.
 * CONCLUSÃO DEDUTIVA: A célula é garantidamente segura.
 /*/
void find_safe_cell(minesweeper_t* ms, int* r, int* c)
{
    *r = -1; // Inicializa como não encontrado
    *c = -1; // Inicializa como não encontrado

    // Loop para testar cada célula do tabuleiro como candidata
    for (int i = 0; i < ms->rows; i++) {
        for (int j = 0; j < ms->cols; j++) {
            
            // Se já está revelada, ignora (já sabemos que é segura)
            if (ms->revealed_grid[i][j]) continue;

            // Preparação do arquivo CNF para o CaDiCaL
            // CONTAGEM PRÉVIA DE CLÁUSULAS
            // O formato DIMACS exige o número exato de cláusulas no cabeçalho,se esse não for o caso,farei isso apenas por garantia.
            // Chamamos a função com NULL apenas para contar as regras do tabuleiro.
            // Somamos +1 para incluir a cláusula da nossa Hipótese ("var 0").
            int num_clausulas = 1 + gerar_regras(NULL, ms);

            // Criação do arquivo físico para que o CaDiCaL o leia
            FILE* f = fopen("output.cnf", "w");
            if (f == NULL) {
                fprintf(stderr, "Erro ao abrir output.cnf\n"); // Boa prática de checagem de erro
                exit(1);
            }

            // Escreve o cabeçalho com a contagem exata calculada acima, por precaução.
            fprintf(f, "p cnf %d %d\n", ms->rows * ms->cols, num_clausulas);

            // Escrevendo a hipótese (Prova por Contradição)
            // Assumimos que a célula (i,j) é uma bomba.
            int var_candidata = coords_to_var(ms->cols, i, j);
            fprintf(f, "%d 0\n", var_candidata);//positivo para indicar que"é bomba".

            // Escreve as regras do jogo (Conhecimento)
            gerar_regras(f, ms);

            fclose(f);

            // Invocação do solucionador SAT 

            // Invocamos o CaDiCaL via pipe para ler sua saída
            // A flag -q (quiet) reduz o ruído no terminal
            FILE* pipe = popen("./extern/cadical/build/cadical -q output.cnf", "r");
            if (pipe == NULL) {
                fprintf(stderr, "Erro ao executar CaDiCaL\n");
                exit(1);
            }

            // Prevenção de perda de dados do pipe
            // É crucial ler toda a saída do solver antes de fechar o pipe.
            // Se fecharmos enquanto ele ainda tenta escrever, o processo se encerra
            // com sinal 141 e perdemos o código de retorno real (10 ou 20).
            char* log_cadical = copy_cadical_output(pipe);
            free(log_cadical); // Descartamos o texto, só queríamos esvaziar o buffer

            int status = pclose(pipe);// Fecha o pipe e armazena o status de saída do processo
            int codigo_saida = -1;

            if (WIFEXITED(status)) {//Usando o manual do CaDiCaL para interpretar o código de saída
                codigo_saida = WEXITSTATUS(status);
            }

            // Interpretação do resultado do CaDiCaL por lógica proposicional
            // Código 20 (UNSAT) = Contradição Encontrada(Vide Manual do CaDiCaL)
            // Significa: "É impossível que essa célula seja uma bomba".
            // Logo, ela é segura para escolher, jogada ideal.
            if (codigo_saida == 20) {
                *r = i;//Atribuindo a jogada segura encontrada
                *c = j;//Atribuindo a jogada segura encontrada
                remove("output.cnf");// Limpeza do arquivo temporário
                return; // Encontramos uma jogada segura, retornamos imediatamente.
            }
            
            // Se for 10 (SAT), a hipótese é possível (pode ser bomba), então não arriscamos.
            // O loop continua para testar a próxima célula.
        }
    }

    // Se chegou aqui, o solver retornou SAT para todas as células ainda não reveladas.
    // Nenhuma jogada segura pôde ser provada logicamente. O main chamará o Oráculo.
    remove("output.cnf");// Limpeza do arquivo temporário
}
/**
 * @brief Função principal do programa.
 *
 * Responsável por inicializar o jogo, controlar o loop principal de jogadas,
 * interagir com o resolvedor SAT e exibir o progresso do tabuleiro no terminal.
 *
 * @param argc Número de argumentos passados pela linha de comando.
 * @param argv Vetor de strings contendo os caminhos dos arquivos de entrada.
 * Espera-se dois argumentos:
 * argv[1] -> Caminho para o arquivo do estado inicial
 * argv[2] -> Caminho para o arquivo com a configuração real
 *
 * @return int Código de saída:
 * - 0 -> execução bem-sucedida (todas as células seguras reveladas)
 * - 1 -> erro ou bomba revelada
 *
 * @details
 * - Exibe o estado inicial do tabuleiro.
 * - Em cada iteração:
 * 1. Tenta encontrar uma célula segura via `find_r_and_c()`.
 * 2. Caso não encontre, recorre ao oráculo (`ms_oracle_cell_decision()`).
 * 3. Revela a célula e atualiza o estado do jogo.
 * - O loop termina quando todas as células não-minadas são reveladas.
 * - Em caso de bomba encontrada, o programa termina imediatamente.
 *
 * @note Caso o número de argumentos esteja incorreto, o programa
 * imprime uma mensagem de uso e encerra com erro.
 */
int main(int argc, char* argv[])
{
  if (argc != 3) {
    fprintf(
      stderr,
      "Número de argumentos inválido!\nUse: ./sat-minesweeper initial-grid-path.in "
      "ground-truth-path.in \n"
    );
    exit(1);
  }

  char* initial_grid_path = argv[1];
  char* ground_truth_path = argv[2];
  minesweeper_t* ms = ms_create(initial_grid_path, ground_truth_path);

  do {
    printf("Estado atual:\n");
    ms_print(ms);
    int r = -1, c = -1;

    find_safe_cell(ms, &r, &c);

    if (r == -1 || c == -1) {
      printf(
        "\nNenhuma célula segura para jogar foi apontada, jogando com o oráculo nessa rodada..."
      );
      ms_oracle_cell_decision(ms, &r, &c);
    }

    printf("\nRevelando a célula [%d, %d]...\n\n", r, c);

    if (!ms_reveal(ms, r, c)) {
      ms_destroy(ms);
      fprintf(stderr, "Bomba encontrada!\nFinalizando o jogo...\n");
      exit(1);
    }
  } while (!ms_check_win(ms));

  printf("Fim!\n");

  printf("Estado final do tabuleiro:\n\n");
  ms_print(ms);

  ms_destroy(ms);

  return 0;
}
