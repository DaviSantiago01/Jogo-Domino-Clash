
#include <raylib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#define CloseWindow CloseWindow_Win
#define ShowCursor ShowCursor_Win
#define DrawText DrawText_Win
#define Rectangle Rectangle_Win
#include <curl/curl.h>
#include <cjson/cJSON.h>
#undef CloseWindow
#undef ShowCursor
#undef DrawText
#undef Rectangle

// Configuração da API Groq - substitua "sua_chave_api" pela sua chave real
// A API utiliza o modelo Llama 3.3 70B para decisões inteligentes da IA
#define GROQ_API_KEY "sua_chave_api"
#define GROQ_API_URL "https://api.groq.com/openai/v1/chat/completions"
#define GROQ_MODEL "llama-3.3-70b-versatile"
#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 720
#define PECA_WIDTH 60
#define PECA_HEIGHT 120

// Estrutura que representa uma peça de dominó
typedef struct
{
    int lado1, lado2; // Valores de cada lado da peça (0-6)
} Peca;

// Lista duplamente encadeada - usado no tabuleiro
typedef struct No
{
    Peca peca;
    struct No *prox, *ant;
} No;

typedef struct
{
    No *inicio, *fim;
    int pontoInicio, pontoFim;
} Tabuleiro;

// Lista encadeada simples - usado na mão dos jogadores
// Cada nó armazena uma peça e um ponteiro para a próxima
// Permite inserção e remoção dinâmica de peças durante o jogo
typedef struct NoMao
{
    Peca peca;
    struct NoMao *proximo;
} NoMao;

typedef struct
{
    NoMao *pecas;
    int quantidade;
} Mao;

// Pilha LIFO (Last In, First Out) - monte de compras
// As últimas peças inseridas são as primeiras a serem retiradas
// Comportamento similar a uma pilha de pratos
typedef struct
{
    Peca pecas[16];
    int topo;
} Monte;

typedef struct
{
    int jogador;
    Peca peca;
    char lado, tipo;
} Jogada;

typedef struct
{
    Jogada jogadas[100];
    int topo;
} Historico;

typedef enum
{
    TELA_MENU,
    TELA_JOGO,
    TELA_REGRAS,
    TELA_SOBRE,
    TELA_FIM
} EstadoJogo;

EstadoJogo estadoAtual = TELA_MENU;
Tabuleiro tabuleiro;
Mao maoHumano, maoIA;
Monte monte;
Historico historico;
int turnoAtual = 1;
int passadas = 0;
int vencedor = 0;
int pecaSelecionada = -1;
char ladoEscolhido = 'E';
char mensagem[256] = "";
int tempoMensagem = 0;
Peca ultimaPecaIA = {-1, -1};
char ultimoLadoIA = '-';
int mostrarJogadaIA = 0;
int tempoJogadaIA = 0;

typedef struct
{
    char *data;
    size_t size;
} HttpResponse;

void inicializar(Tabuleiro *tab, Mao *j1, Mao *j2, Monte *monte, Historico *hist)
{
    tab->inicio = tab->fim = NULL;
    tab->pontoInicio = tab->pontoFim = -1;
    j1->pecas = NULL;
    j1->quantidade = 0;
    j2->pecas = NULL;
    j2->quantidade = 0;
    monte->topo = -1;
    hist->topo = -1;
}
// Adiciona uma peça no início da lista (mão do jogador)
// Operação O(1) - inserção na cabeça da lista
void adicionarPeca(Mao *mao, Peca peca)
{
    NoMao *novo = (NoMao *)malloc(sizeof(NoMao));
    if (!novo)
        return;
    novo->peca = peca;
    novo->proximo = mao->pecas;
    mao->pecas = novo;
    mao->quantidade++;
}
// Remove uma peça específica da mão do jogador
// Busca linear O(n) - percorre a lista até encontrar a peça
int removerPeca(Mao *mao, int lado1, int lado2)
{
    NoMao *atual = mao->pecas, *anterior = NULL;
    while (atual)
    {
        if ((atual->peca.lado1 == lado1 && atual->peca.lado2 == lado2) ||
            (atual->peca.lado1 == lado2 && atual->peca.lado2 == lado1))
        {
            if (anterior)
                anterior->proximo = atual->proximo;
            else
                mao->pecas = atual->proximo;
            free(atual);
            mao->quantidade--;
            return 1;
        }
        anterior = atual;
        atual = atual->proximo;
    }
    return 0;
}
// Bubble Sort - ordena peças da mão por valor (O(n²))
void ordenarMao(Mao *mao)
{
    if (mao->quantidade <= 1)
        return;

    Peca temp[20];
    NoMao *atual = mao->pecas;
    int i = 0;

    while (atual && i < 20)
    {
        temp[i] = atual->peca;
        atual = atual->proximo;
        i++;
    }

    int total = i;

    // Loop externo - passa por todos os elementos
    for (i = 0; i < total - 1; i++)
    {
        // Loop interno - compara elementos adjacentes e troca se necessário
        for (int j = 0; j < total - i - 1; j++)
        {
            int valor1 = temp[j].lado1 + temp[j].lado2;
            int valor2 = temp[j + 1].lado1 + temp[j + 1].lado2;

            if (valor1 < valor2)
            {
                Peca aux = temp[j];
                temp[j] = temp[j + 1];
                temp[j + 1] = aux;
            }
        }
    }

    while (mao->pecas)
    {
        NoMao *aux = mao->pecas;
        mao->pecas = mao->pecas->proximo;
        free(aux);
    }

    mao->pecas = NULL;
    mao->quantidade = 0;

    for (i = 0; i < total; i++)
        adicionarPeca(mao, temp[i]);
}
// Calcula a pontuação total somando os valores de todas as peças
// Percorre a lista encadeada acumulando os valores
int calcularPontos(Mao *mao)
{
    int total = 0;
    for (NoMao *atual = mao->pecas; atual; atual = atual->proximo)
        total += atual->peca.lado1 + atual->peca.lado2;
    return total;
}
// Verifica se uma peça pode ser encaixada no tabuleiro
// Checa se algum lado da peça corresponde às pontas do tabuleiro
int encaixa(Tabuleiro *tab, Peca peca)
{
    if (!tab->inicio)
        return 1;
    return (peca.lado1 == tab->pontoInicio || peca.lado2 == tab->pontoInicio ||
            peca.lado1 == tab->pontoFim || peca.lado2 == tab->pontoFim);
}
// Insere peça no tabuleiro (lista duplamente encadeada)
// Permite inserção na esquerda ou direita, invertendo a peça se necessário
int inserir(Tabuleiro *tab, Peca peca, char lado)
{
    No *novo = (No *)malloc(sizeof(No));
    if (!novo)
        return 0;
    novo->peca = peca;
    novo->prox = novo->ant = NULL;

    // Primeira peça do tabuleiro
    if (!tab->inicio)
    {
        tab->inicio = tab->fim = novo;
        tab->pontoInicio = peca.lado1;
        tab->pontoFim = peca.lado2;
        return 1;
    }

    // Inserção na esquerda
    if (lado == 'E' || lado == 'e')
    {
        if (peca.lado2 == tab->pontoInicio)
        {
            novo->prox = tab->inicio;
            tab->inicio->ant = novo;
            tab->inicio = novo;
            tab->pontoInicio = peca.lado1;
            return 1;
        }
        else if (peca.lado1 == tab->pontoInicio)
        {
            // Inverte a peça para encaixar
            novo->peca.lado1 = peca.lado2;
            novo->peca.lado2 = peca.lado1;
            novo->prox = tab->inicio;
            tab->inicio->ant = novo;
            tab->inicio = novo;
            tab->pontoInicio = novo->peca.lado1;
            return 1;
        }
    }

    // Inserção na direita
    if (lado == 'D' || lado == 'd')
    {
        if (peca.lado1 == tab->pontoFim)
        {
            tab->fim->prox = novo;
            novo->ant = tab->fim;
            tab->fim = novo;
            tab->pontoFim = peca.lado2;
            return 1;
        }
        else if (peca.lado2 == tab->pontoFim)
        {
            // Inverte a peça para encaixar
            novo->peca.lado1 = peca.lado2;
            novo->peca.lado2 = peca.lado1;
            tab->fim->prox = novo;
            novo->ant = tab->fim;
            tab->fim = novo;
            tab->pontoFim = novo->peca.lado2;
            return 1;
        }
    }
    free(novo);
    return 0;
}
int contarJogadas(Mao *mao, Tabuleiro *tab)
{
    int count = 0;
    for (NoMao *atual = mao->pecas; atual; atual = atual->proximo)
        if (encaixa(tab, atual->peca))
            count++;
    return count;
}
// Cria as 28 peças do dominó (0-0 até 6-6) e embaralha
void embaralhar(Peca baralho[28])
{
    int index = 0;
    // Gera todas as combinações possíveis
    for (int i = 0; i <= 6; i++)
        for (int j = i; j <= 6; j++)
            baralho[index++] = (Peca){i, j};
    // Algoritmo Fisher-Yates para embaralhar
    // Garante distribuição uniforme - cada peça tem chance igual de ficar em qualquer posição
    for (int i = 27; i > 0; i--)
    {
        int j = rand() % (i + 1);
        Peca temp = baralho[i];
        baralho[i] = baralho[j];
        baralho[j] = temp;
    }
}
// Distribui 6 peças para cada jogador e restante vai para o monte
void distribuir(Mao *j1, Mao *j2, Monte *monte)
{
    Peca baralho[28];
    embaralhar(baralho);
    // Cada jogador recebe 6 peças
    for (int i = 0; i < 6; i++)
        adicionarPeca(j1, baralho[i]);
    for (int i = 6; i < 12; i++)
        adicionarPeca(j2, baralho[i]);
    // Restante das peças (16) vai para a pilha do monte
    for (int i = 12; i < 28; i++)
        monte->pecas[++monte->topo] = baralho[i];
}
// Compra uma peça do monte (operação POP da pilha)
int comprar(Monte *monte, Peca *peca)
{
    if (monte->topo < 0)
        return 0;
    *peca = monte->pecas[monte->topo--];
    return 1;
}
void registrar(Historico *hist, int jogador, Peca peca, char lado, char tipo)
{
    if (hist->topo >= 99)
        return;
    hist->jogadas[++hist->topo] = (Jogada){jogador, peca, lado, tipo};
}
void mostrarMensagem(const char *msg)
{
    strncpy(mensagem, msg, 255);
    mensagem[255] = '\0';
    tempoMensagem = 420; // 7 segundos a 60 FPS
}

// Função callback para receber dados da requisição HTTP
// Chamada automaticamente pela libcurl conforme a resposta chega
static size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t realsize = size * nmemb;
    HttpResponse *mem = (HttpResponse *)userp;

    char *ptr = realloc(mem->data, mem->size + realsize + 1);
    if (!ptr)
    {
        return 0;
    }

    mem->data = ptr;
    memcpy(&(mem->data[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->data[mem->size] = 0;

    return realsize;
}

// Constrói o prompt contextualizado para enviar à IA
// Inclui as peças disponíveis e o estado atual do tabuleiro
char *construirPromptIA(Mao *maoIA, Tabuleiro *tab)
{
    static char prompt[2048], temp[64];
    char pecas[512];

    strcpy(pecas, "[");

    for (NoMao *atual = maoIA->pecas; atual; atual = atual->proximo)
    {
        snprintf(temp, 64, "%s[%d|%d]", pecas[1] ? ", " : "", atual->peca.lado1, atual->peca.lado2);
        strcat(pecas, temp);
    }
    strcat(pecas, "]");

    if (!tab->inicio)
        snprintf(prompt, 2048, "DOMINÓ - Mesa vazia\nSuas pecas: %s\n\nEscolha qualquer peca.\nResposta: [numero1|numero2]\nExemplo: [6|4]", pecas);
    else
        snprintf(prompt, 2048, "DOMINÓ\nSuas pecas: %s\nMesa: esquerda=%d direita=%d\n\nVOCE PRECISA:\n1. Escolher UMA peca sua que tenha o numero %d OU %d\n2. Decidir o lado: E (esquerda) ou D (direita)\n\nCOMO JOGAR:\n- Para jogar na ESQUERDA: sua peca PRECISA ter o numero %d\n- Para jogar na DIREITA: sua peca PRECISA ter o numero %d\n\nEXEMPLO PRATICO:\nSe voce tem [%d|2] e quer jogar na esquerda: [%d|2] E\nSe voce tem [1|%d] e quer jogar na direita: [1|%d] D\n\nResposta (formato obrigatorio): [numero1|numero2] LADO", pecas, tab->pontoInicio, tab->pontoFim, tab->pontoInicio, tab->pontoFim, tab->pontoInicio, tab->pontoFim, tab->pontoInicio, tab->pontoInicio, tab->pontoFim, tab->pontoFim);

    return prompt;
}

// Requisição HTTP para API Groq (libcurl + cJSON)
char *chamarGroqAPI(const char *prompt)
{
    CURL *curl;
    CURLcode res;
    HttpResponse chunk = {0};
    chunk.data = malloc(1);
    chunk.size = 0;

    curl = curl_easy_init();
    if (!curl)
    {
        free(chunk.data);
        return NULL;
    }

    cJSON *root = cJSON_CreateObject();
    cJSON *messages = cJSON_CreateArray();

    cJSON *system_msg = cJSON_CreateObject();
    cJSON_AddItemToObject(system_msg, "role", cJSON_CreateString("system"));
    cJSON_AddItemToObject(system_msg, "content", cJSON_CreateString("Voce e um jogador expert de dominó. Analise as pecas disponiveis e escolha a melhor jogada possivel. Responda APENAS no formato [numero1|numero2] LADO. Nao explique, nao justifique, apenas responda."));
    cJSON_AddItemToArray(messages, system_msg);

    cJSON *user_msg = cJSON_CreateObject();
    cJSON_AddItemToObject(user_msg, "role", cJSON_CreateString("user"));
    cJSON_AddItemToObject(user_msg, "content", cJSON_CreateString(prompt));
    cJSON_AddItemToArray(messages, user_msg);

    cJSON_AddItemToObject(root, "model", cJSON_CreateString(GROQ_MODEL));
    cJSON_AddItemToObject(root, "messages", messages);
    cJSON_AddItemToObject(root, "temperature", cJSON_CreateNumber(0.0));
    cJSON_AddItemToObject(root, "max_tokens", cJSON_CreateNumber(20));
    cJSON_AddItemToObject(root, "top_p", cJSON_CreateNumber(1.0));

    char *json_str = cJSON_Print(root);

    struct curl_slist *headers = NULL;
    char auth_header[512];
    snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", GROQ_API_KEY);
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, auth_header);

    curl_easy_setopt(curl, CURLOPT_URL, GROQ_API_URL);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_str);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

    res = curl_easy_perform(curl);

    char *response = NULL;
    if (res == CURLE_OK)
    {
        response = strdup(chunk.data);
    }

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    cJSON_Delete(root);
    free(json_str);
    free(chunk.data);

    return response;
}

// Extrai a jogada da resposta JSON da API Groq
int parseResposta(const char *json_response, Peca *peca, char *lado)
{
    if (!json_response)
        return 0;
    cJSON *root = cJSON_Parse(json_response);
    if (!root)
        return 0;

    // Navega na estrutura JSON para obter a mensagem da IA
    cJSON *message = cJSON_GetObjectItem(cJSON_GetArrayItem(
                                             cJSON_GetObjectItem(root, "choices"), 0),
                                         "message");

    if (!message)
    {
        cJSON_Delete(root);
        return 0;
    }

    cJSON *content = cJSON_GetObjectItem(message, "content");
    const char *resposta = NULL;

    if (content && cJSON_IsString(content) && strlen(content->valuestring) > 0)
    {
        resposta = content->valuestring;
    }
    else
    {

        cJSON *reasoning = cJSON_GetObjectItem(message, "reasoning");
        if (reasoning && cJSON_IsString(reasoning))
        {
            resposta = reasoning->valuestring;
        }
    }

    if (!resposta || strlen(resposta) == 0)
    {
        cJSON_Delete(root);
        return 0;
    }

    int lado1, lado2;
    char lado_char, lado_str[20];

    // Tenta parsear diferentes formatos de resposta: [6|4] E, [6|4] esquerda, [6|4]
    if (sscanf(resposta, "[%d|%d] %c", &lado1, &lado2, &lado_char) == 3)
    {
        *peca = (Peca){lado1, lado2};
        *lado = (lado_char == 'E' || lado_char == 'e') ? 'E' : 'D';
    }
    else if (sscanf(resposta, "[%d|%d] %19s", &lado1, &lado2, lado_str) == 3)
    {
        *peca = (Peca){lado1, lado2};
        *lado = (strstr(lado_str, "esq") || lado_str[0] == 'E' || lado_str[0] == 'e') ? 'E' : 'D';
    }
    else if (sscanf(resposta, "[%d|%d]", &lado1, &lado2) == 2)
    {
        *peca = (Peca){lado1, lado2};
        *lado = 'D';
    }
    else
    {
        cJSON_Delete(root);
        return 0;
    }

    cJSON_Delete(root);
    return 1;
}

void desenharPeca(int x, int y, Peca peca, Color cor)
{
    DrawRectangle(x, y, PECA_WIDTH, PECA_HEIGHT, cor);
    DrawRectangleLines(x, y, PECA_WIDTH, PECA_HEIGHT, BLACK);
    DrawLine(x, y + PECA_HEIGHT / 2, x + PECA_WIDTH, y + PECA_HEIGHT / 2, BLACK);
    DrawText(TextFormat("%d", peca.lado1), x + PECA_WIDTH / 2 - 10, y + 20, 30, BLACK);
    DrawText(TextFormat("%d", peca.lado2), x + PECA_WIDTH / 2 - 10, y + PECA_HEIGHT / 2 + 20, 30, BLACK);
}

void desenharTabuleiro()
{
    DrawRectangle(0, 0, SCREEN_WIDTH, 200, DARKGREEN);
    DrawText("MESA", 20, 20, 30, WHITE);

    if (!tabuleiro.inicio)
    {
        DrawText("Mesa vazia - Primeira jogada", 350, 90, 25, YELLOW);
        return;
    }

    int totalPecas = 0, x = 50;
    for (No *c = tabuleiro.inicio; c; c = c->prox)
        totalPecas++;

    if (totalPecas <= 8)
    {
        for (No *atual = tabuleiro.inicio; atual; atual = atual->prox, x += PECA_WIDTH + 10)
            desenharPeca(x, 50, atual->peca, WHITE);
    }
    else
    {
        No *atual = tabuleiro.inicio;
        for (int i = 0; i < 3 && atual; i++, atual = atual->prox, x += PECA_WIDTH + 10)
            desenharPeca(x, 50, atual->peca, WHITE);
        DrawText("...", x + 10, 90, 40, WHITE);
        x += 50;
        No *temp[3] = {NULL, NULL, NULL};
        No *p = tabuleiro.fim;
        for (int i = 2; i >= 0 && p; i--, p = p->ant)
            temp[i] = p;
        for (int i = 0; i < 3; i++)
            if (temp[i])
            {
                desenharPeca(x, 50, temp[i]->peca, WHITE);
                x += PECA_WIDTH + 10;
            }
    }

    DrawText(TextFormat("Pontas: [%d] e [%d]", tabuleiro.pontoInicio, tabuleiro.pontoFim), 850, 20, 25, YELLOW);
    DrawText(TextFormat("Total: %d pecas", totalPecas), 850, 55, 20, LIGHTGRAY);
}

void desenharMaoHumano()
{
    DrawText("SUA MAO", 20, 220, 30, WHITE);
    DrawText(TextFormat("Pecas: %d | Pontos: %d", maoHumano.quantidade, calcularPontos(&maoHumano)), 20, 260, 20, LIGHTGRAY);

    int x = 50, i = 0;
    for (NoMao *atual = maoHumano.pecas; atual; atual = atual->proximo, i++, x += PECA_WIDTH + 15)
    {
        desenharPeca(x, 300, atual->peca, i == pecaSelecionada ? YELLOW : LIGHTGRAY);
        if (encaixa(&tabuleiro, atual->peca))
            DrawCircle(x + PECA_WIDTH - 10, 310, 8, GREEN);
    }
}

void desenharMaoIA()
{
    DrawText("MAO DA IA", SCREEN_WIDTH - 200, 220, 25, WHITE);
    DrawText(TextFormat("Pecas: %d", maoIA.quantidade), SCREEN_WIDTH - 200, 260, 20, LIGHTGRAY);
    for (int i = 0; i < maoIA.quantidade && i < 6; i++)
    {
        DrawRectangle(SCREEN_WIDTH - 200, 300 + i * 25, 50, 20, GRAY);
        DrawRectangleLines(SCREEN_WIDTH - 200, 300 + i * 25, 50, 20, BLACK);
    }
}

void desenharBotoes()
{
    Color corE = (ladoEscolhido == 'E') ? GREEN : LIGHTGRAY;
    DrawRectangle(50, 500, 150, 50, corE);
    DrawText("ESQUERDA (E)", 60, 515, 20, BLACK);

    Color corD = (ladoEscolhido == 'D') ? GREEN : LIGHTGRAY;
    DrawRectangle(220, 500, 150, 50, corD);
    DrawText("DIREITA (D)", 235, 515, 20, BLACK);

    DrawRectangle(50, 570, 150, 50, BLUE);
    DrawText("JOGAR", 90, 585, 25, WHITE);

    DrawRectangle(220, 570, 150, 50, ORANGE);
    DrawText("COMPRAR", 245, 585, 20, WHITE);

    DrawRectangle(SCREEN_WIDTH - 180, 500, 160, 50, RED);
    DrawText("VOLTAR MENU", SCREEN_WIDTH - 165, 515, 20, WHITE);

    DrawText(TextFormat("Monte: %d", monte.topo + 1), 50, 640, 20, WHITE);
}

void desenharMensagem()
{
    if (tempoMensagem-- > 0)
    {
        DrawRectangle(SCREEN_WIDTH / 2 - 250, SCREEN_HEIGHT - 100, 500, 60, Fade(BLACK, 0.8f));
        DrawText(mensagem, SCREEN_WIDTH / 2 - 240, SCREEN_HEIGHT - 85, 20, WHITE);
    }
}

// Renderiza a tela do menu principal
// Interface gráfica criada com funções de desenho da Raylib
void desenharMenu()
{
    DrawText("DOMINÓ CLASH", SCREEN_WIDTH / 2 - 200, 80, 60, GOLD);
    DrawText("Humano vs IA Estrategica", SCREEN_WIDTH / 2 - 150, 160, 25, WHITE);

    DrawRectangle(SCREEN_WIDTH / 2 - 150, 240, 300, 60, BLUE);
    DrawText("JOGAR", SCREEN_WIDTH / 2 - 60, 258, 30, WHITE);

    DrawRectangle(SCREEN_WIDTH / 2 - 150, 320, 300, 60, GREEN);
    DrawText("REGRAS", SCREEN_WIDTH / 2 - 70, 338, 30, WHITE);

    DrawRectangle(SCREEN_WIDTH / 2 - 150, 400, 300, 60, PURPLE);
    DrawText("SOBRE", SCREEN_WIDTH / 2 - 60, 418, 30, WHITE);

    DrawRectangle(SCREEN_WIDTH / 2 - 150, 480, 300, 50, RED);
    DrawText("SAIR", SCREEN_WIDTH / 2 - 35, 495, 25, WHITE);

    DrawText("Nova(Velha) InfancIA - 2025", SCREEN_WIDTH / 2 - 150, 660, 18, LIGHTGRAY);
}

// Exibe as regras do jogo de forma visual e organizada
void desenharRegras()
{
    DrawText("REGRAS DO DOMINÓ", SCREEN_WIDTH / 2 - 200, 50, 40, GOLD);

    DrawText("OBJETIVO:", 50, 120, 25, WHITE);
    DrawText("Ser o primeiro a ficar sem pecas", 50, 150, 20, LIGHTGRAY);

    DrawText("COMO JOGAR:", 50, 200, 25, WHITE);
    DrawText("- Cada jogador comeca com 6 pecas", 50, 230, 20, LIGHTGRAY);
    DrawText("- Encaixe pecas pelos numeros iguais", 50, 260, 20, LIGHTGRAY);
    DrawText("- Escolha ESQUERDA ou DIREITA para jogar", 50, 290, 20, LIGHTGRAY);
    DrawText("- Sem jogadas validas? Compre do monte", 50, 320, 20, LIGHTGRAY);

    DrawText("VITORIA:", 50, 370, 25, WHITE);
    DrawText("- Vence quem ficar sem pecas primeiro", 50, 400, 20, LIGHTGRAY);
    DrawText("- Se o monte acabar, vence quem tiver", 50, 430, 20, LIGHTGRAY);
    DrawText("  menos pontos nas pecas restantes", 50, 460, 20, LIGHTGRAY);

    DrawRectangle(SCREEN_WIDTH / 2 - 100, 550, 200, 50, BLUE);
    DrawText("VOLTAR", SCREEN_WIDTH / 2 - 50, 565, 25, WHITE);
}

void desenharSobre()
{
    DrawText("SOBRE O PROJETO", SCREEN_WIDTH / 2 - 220, 20, 40, GOLD);

    DrawText("AUTORES:", 50, 70, 23, WHITE);
    DrawText("Davi Santiago e Jose Jorge", 50, 95, 18, LIGHTGRAY);

    DrawText("ESTRUTURAS DE DADOS:", 50, 135, 23, WHITE);
    DrawText("Lista Duplamente Encadeada - Tabuleiro", 50, 160, 17, LIGHTGRAY);
    DrawText("  (permite navegar pecas pra frente/tras)", 50, 180, 15, DARKGRAY);
    DrawText("Lista Encadeada Simples - Maos dos jogadores", 50, 205, 17, LIGHTGRAY);
    DrawText("  (adicionar/remover pecas dinamicamente)", 50, 225, 15, DARKGRAY);
    DrawText("Pilha LIFO - Monte de compras", 50, 250, 17, LIGHTGRAY);
    DrawText("  (ultima peca colocada eh a primeira comprada)", 50, 270, 15, DARKGRAY);
    DrawText("Bubble Sort - Ordenacao de pecas por valor", 50, 295, 17, LIGHTGRAY);

    DrawText("TECNOLOGIAS:", 50, 335, 23, WHITE);
    DrawText("Linguagem C - Codigo principal", 50, 360, 17, LIGHTGRAY);
    DrawText("Raylib 5.0 - Interface grafica", 50, 382, 17, LIGHTGRAY);
    DrawText("libcurl - Requisicoes HTTP para API", 50, 404, 17, LIGHTGRAY);
    DrawText("cJSON - Parse de respostas JSON", 50, 426, 17, LIGHTGRAY);

    DrawText("INTELIGENCIA ARTIFICIAL:", 50, 466, 23, WHITE);
    DrawText("Groq API - Servico de IA em nuvem", 50, 491, 17, LIGHTGRAY);
    DrawText("Modelo: Llama 3.3 70B Versatile", 50, 513, 17, LIGHTGRAY);
    DrawText("IA analisa pecas e decide melhor jogada", 50, 535, 17, LIGHTGRAY);

    DrawRectangle(SCREEN_WIDTH / 2 - 100, 600, 200, 50, BLUE);
    DrawText("VOLTAR", SCREEN_WIDTH / 2 - 50, 615, 25, WHITE);
}

void desenharTelaFim()
{
    DrawText("FIM DE JOGO!", SCREEN_WIDTH / 2 - 180, 100, 50, GOLD);

    if (vencedor == 1)
    {
        DrawText("VOCE VENCEU!", SCREEN_WIDTH / 2 - 150, 200, 40, GREEN);
    }
    else if (vencedor == 2)
    {
        DrawText("IA VENCEU!", SCREEN_WIDTH / 2 - 120, 200, 40, RED);
    }
    else
    {
        DrawText("EMPATE!", SCREEN_WIDTH / 2 - 90, 200, 40, YELLOW);
    }

    DrawText(TextFormat("Sua pontuacao: %d pts (%d pecas)",
                        calcularPontos(&maoHumano), maoHumano.quantidade),
             SCREEN_WIDTH / 2 - 220, 300, 23, WHITE);
    DrawText(TextFormat("IA: %d pts (%d pecas)",
                        calcularPontos(&maoIA), maoIA.quantidade),
             SCREEN_WIDTH / 2 - 220, 340, 23, WHITE);

    DrawRectangle(SCREEN_WIDTH / 2 - 100, 450, 200, 50, BLUE);
    DrawText("MENU", SCREEN_WIDTH / 2 - 40, 465, 25, WHITE);
}

void iniciarJogo()
{
    inicializar(&tabuleiro, &maoHumano, &maoIA, &monte, &historico);
    distribuir(&maoHumano, &maoIA, &monte);
    ordenarMao(&maoHumano);
    ordenarMao(&maoIA);
    turnoAtual = 1;
    passadas = 0;
    vencedor = 0;
    pecaSelecionada = -1;
    ladoEscolhido = 'E';
    mostrarJogadaIA = 0;
    tempoJogadaIA = 0;
    ultimaPecaIA.lado1 = -1;
    ultimaPecaIA.lado2 = -1;
    ultimoLadoIA = '-';
    mostrarMensagem("Jogo iniciado! Sua vez!");
}

// Verifica as condições de vitória do jogo
// Retorna: 1 = humano venceu, 2 = IA venceu, 3 = empate, 0 = jogo continua
int verificarVitoria()
{
    if (maoHumano.quantidade == 0)
        return 1;
    if (maoIA.quantidade == 0)
        return 2;
    if (passadas >= 2)
    {
        int pts1 = calcularPontos(&maoHumano);
        int pts2 = calcularPontos(&maoIA);
        if (pts1 < pts2)
            return 1;
        if (pts2 < pts1)
            return 2;
        return 3;
    }
    return 0;
}

// Processa o turno da IA: chama API Groq ou usa fallback local
void processarTurnoIA()
{
    // Se IA não tem jogadas válidas, compra ou passa
    if (contarJogadas(&maoIA, &tabuleiro) == 0)
    {
        Peca nova;
        if (comprar(&monte, &nova))
        {
            adicionarPeca(&maoIA, nova);
            ordenarMao(&maoIA);
            mostrarMensagem("IA comprou uma peca");
            registrar(&historico, 2, nova, '-', 'C');
        }
        else
        {
            mostrarMensagem("IA passou a vez");
            passadas++;
            registrar(&historico, 2, (Peca){0, 0}, '-', 'P');
        }
        turnoAtual = 1;
        return;
    }

    printf("[IA] Processando turno...\n");

    char *prompt = construirPromptIA(&maoIA, &tabuleiro);
    char *resposta_json = chamarGroqAPI(prompt);

    // Sistema de fallback triplo: garante que a IA sempre faça uma jogada válida
    // Fallback 1: Se a API não responder, usa algoritmo local
    if (!resposta_json)
    {
        printf("[IA] Falha na API - usando fallback local\n");

        // IA local simples: joga primeira peça válida que encontrar
        NoMao *atual = maoIA.pecas;
        while (atual)
        {
            if (encaixa(&tabuleiro, atual->peca))
            {
                char lado = 'D';
                if (tabuleiro.inicio)
                {
                    if (atual->peca.lado1 == tabuleiro.pontoInicio ||
                        atual->peca.lado2 == tabuleiro.pontoInicio)
                    {
                        lado = 'E';
                    }
                }

                if (inserir(&tabuleiro, atual->peca, lado))
                {
                    ultimaPecaIA = atual->peca;
                    ultimoLadoIA = lado;
                    mostrarJogadaIA = 1;
                    tempoJogadaIA = 180;

                    printf("[IA] Jogada executada (fallback)\n");

                    registrar(&historico, 2, atual->peca, lado, 'J');
                    removerPeca(&maoIA, atual->peca.lado1, atual->peca.lado2);
                    passadas = 0;
                    break;
                }
            }
            atual = atual->proximo;
        }
        turnoAtual = 1;
        return;
    }

    Peca pecaEscolhida;
    char ladoEscolhido;

    // Valida se a resposta da API é uma jogada válida
    // Verifica se a peça existe na mão da IA e se pode ser encaixada
    if (parseResposta(resposta_json, &pecaEscolhida, &ladoEscolhido))
    {
        int temPeca = 0;
        for (NoMao *atual = maoIA.pecas; atual; atual = atual->proximo)
        {
            if ((atual->peca.lado1 == pecaEscolhida.lado1 && atual->peca.lado2 == pecaEscolhida.lado2) ||
                (atual->peca.lado1 == pecaEscolhida.lado2 && atual->peca.lado2 == pecaEscolhida.lado1))
            {
                temPeca = 1;
                break;
            }
        }

        if (temPeca && encaixa(&tabuleiro, pecaEscolhida))
        {
            if (inserir(&tabuleiro, pecaEscolhida, ladoEscolhido))
            {
                ultimaPecaIA = pecaEscolhida;
                ultimoLadoIA = ladoEscolhido;
                mostrarJogadaIA = 1;
                tempoJogadaIA = 180;

                printf("[IA] Jogada validada pela API\n");

                char msg[128];
                snprintf(msg, sizeof(msg), "IA jogou [%d|%d] na %s",
                         pecaEscolhida.lado1, pecaEscolhida.lado2,
                         ladoEscolhido == 'E' ? "ESQUERDA" : "DIREITA");
                mostrarMensagem(msg);

                registrar(&historico, 2, pecaEscolhida, ladoEscolhido, 'J');
                removerPeca(&maoIA, pecaEscolhida.lado1, pecaEscolhida.lado2);
                passadas = 0;
            }
            else
            {
                printf("[IA] Resposta invalida da API - usando fallback\n");
                NoMao *atual = maoIA.pecas;
                while (atual)
                {
                    if (encaixa(&tabuleiro, atual->peca))
                    {
                        char lado = 'D';
                        if (tabuleiro.inicio)
                        {
                            if (atual->peca.lado1 == tabuleiro.pontoInicio ||
                                atual->peca.lado2 == tabuleiro.pontoInicio)
                            {
                                lado = 'E';
                            }
                        }

                        if (inserir(&tabuleiro, atual->peca, lado))
                        {
                            char msg[128];
                            snprintf(msg, sizeof(msg), "IA jogou [%d|%d] na %s",
                                     atual->peca.lado1, atual->peca.lado2,
                                     lado == 'E' ? "ESQUERDA" : "DIREITA");
                            mostrarMensagem(msg);

                            registrar(&historico, 2, atual->peca, lado, 'J');
                            removerPeca(&maoIA, atual->peca.lado1, atual->peca.lado2);
                            passadas = 0;
                            break;
                        }
                    }
                    atual = atual->proximo;
                }
            }
        }
        else
        {
            printf("[IA] Erro ao validar peca - usando fallback\n");
            NoMao *atual = maoIA.pecas;
            while (atual)
            {
                if (encaixa(&tabuleiro, atual->peca))
                {
                    char lado = 'D';
                    if (tabuleiro.inicio)
                    {
                        if (atual->peca.lado1 == tabuleiro.pontoInicio ||
                            atual->peca.lado2 == tabuleiro.pontoInicio)
                        {
                            lado = 'E';
                        }
                    }

                    if (inserir(&tabuleiro, atual->peca, lado))
                    {
                        char msg[128];
                        snprintf(msg, sizeof(msg), "IA jogou [%d|%d] na %s",
                                 atual->peca.lado1, atual->peca.lado2,
                                 lado == 'E' ? "ESQUERDA" : "DIREITA");
                        mostrarMensagem(msg);

                        registrar(&historico, 2, atual->peca, lado, 'J');
                        removerPeca(&maoIA, atual->peca.lado1, atual->peca.lado2);
                        passadas = 0;
                        break;
                    }
                }
                atual = atual->proximo;
            }
        }
    }
    else
    {
        printf("[IA] Erro ao parsear resposta - usando fallback\n");
        NoMao *atual = maoIA.pecas;
        while (atual)
        {
            if (encaixa(&tabuleiro, atual->peca))
            {
                char lado = 'D';
                if (tabuleiro.inicio)
                {
                    if (atual->peca.lado1 == tabuleiro.pontoInicio ||
                        atual->peca.lado2 == tabuleiro.pontoInicio)
                    {
                        lado = 'E';
                    }
                }

                if (inserir(&tabuleiro, atual->peca, lado))
                {
                    char msg[128];
                    snprintf(msg, sizeof(msg), "IA jogou [%d|%d] na %s",
                             atual->peca.lado1, atual->peca.lado2,
                             lado == 'E' ? "ESQUERDA" : "DIREITA");
                    mostrarMensagem(msg);

                    registrar(&historico, 2, atual->peca, lado, 'J');
                    removerPeca(&maoIA, atual->peca.lado1, atual->peca.lado2);
                    passadas = 0;
                    break;
                }
            }
            atual = atual->proximo;
        }
    }

    free(resposta_json);
    turnoAtual = 1;
}

// Função principal: inicializa janela e loop do jogo
int main()
{
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Domino Clash - Nova(Velha) InfancIA");
    SetTargetFPS(60);
    srand(time(NULL)); // Inicializa gerador de números aleatórios

    while (!WindowShouldClose())
    {
        Vector2 mousePos = GetMousePosition();

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            switch (estadoAtual)
            {
            case TELA_MENU:
                if (CheckCollisionPointRec(mousePos, (Rectangle){SCREEN_WIDTH / 2 - 150, 240, 300, 60}))
                {
                    iniciarJogo();
                    estadoAtual = TELA_JOGO;
                }
                else if (CheckCollisionPointRec(mousePos, (Rectangle){SCREEN_WIDTH / 2 - 150, 320, 300, 60}))
                {
                    estadoAtual = TELA_REGRAS;
                }
                else if (CheckCollisionPointRec(mousePos, (Rectangle){SCREEN_WIDTH / 2 - 150, 400, 300, 60}))
                {
                    estadoAtual = TELA_SOBRE;
                }
                else if (CheckCollisionPointRec(mousePos, (Rectangle){SCREEN_WIDTH / 2 - 150, 480, 300, 50}))
                {
                    CloseWindow();
                    return 0;
                }
                break;

            case TELA_JOGO:
                if (CheckCollisionPointRec(mousePos, (Rectangle){SCREEN_WIDTH - 180, 500, 160, 50}))
                {
                    estadoAtual = TELA_MENU;
                    break;
                }

                // Processamento de cliques do jogador humano
                if (turnoAtual == 1)
                {
                    NoMao *atual = maoHumano.pecas;
                    int x = 50, i = 0;
                    // Detecta clique em uma peça da mão
                    while (atual)
                    {
                        if (CheckCollisionPointRec(mousePos, (Rectangle){x, 300, PECA_WIDTH, PECA_HEIGHT}))
                        {
                            pecaSelecionada = i;
                        }
                        x += PECA_WIDTH + 15;
                        atual = atual->proximo;
                        i++;
                    }

                    if (CheckCollisionPointRec(mousePos, (Rectangle){50, 500, 150, 50}))
                        ladoEscolhido = 'E';
                    else if (CheckCollisionPointRec(mousePos, (Rectangle){220, 500, 150, 50}))
                        ladoEscolhido = 'D';
                    else if (CheckCollisionPointRec(mousePos, (Rectangle){50, 570, 150, 50}))
                    {
                        if (pecaSelecionada >= 0)
                        {
                            NoMao *peca = maoHumano.pecas;
                            for (int j = 0; j < pecaSelecionada; j++)
                                peca = peca->proximo;

                            if (encaixa(&tabuleiro, peca->peca))
                            {
                                if (inserir(&tabuleiro, peca->peca, ladoEscolhido))
                                {
                                    registrar(&historico, 1, peca->peca, ladoEscolhido, 'J');
                                    removerPeca(&maoHumano, peca->peca.lado1, peca->peca.lado2);
                                    ordenarMao(&maoHumano);
                                    pecaSelecionada = -1;
                                    passadas = 0;
                                    turnoAtual = 2;
                                    mostrarMensagem("Jogada realizada!");
                                }
                            }
                            else
                            {
                                mostrarMensagem("Peca nao encaixa!");
                            }
                        }
                        else
                        {
                            mostrarMensagem("Selecione uma peca!");
                        }
                    }
                    else if (CheckCollisionPointRec(mousePos, (Rectangle){220, 570, 150, 50}))
                    {
                        Peca nova;
                        if (comprar(&monte, &nova))
                        {
                            adicionarPeca(&maoHumano, nova);
                            ordenarMao(&maoHumano);
                            registrar(&historico, 1, nova, '-', 'C');
                            mostrarMensagem("Comprou uma peca!");
                        }
                        else
                        {
                            if (contarJogadas(&maoHumano, &tabuleiro) == 0)
                            {
                                mostrarMensagem("Passou a vez");
                                passadas++;
                                turnoAtual = 2;
                                registrar(&historico, 1, (Peca){0, 0}, '-', 'P');
                            }
                            else
                            {
                                mostrarMensagem("Monte vazio!");
                            }
                        }
                    }
                }
                break;

            case TELA_REGRAS:
                if (CheckCollisionPointRec(mousePos, (Rectangle){SCREEN_WIDTH / 2 - 100, 550, 200, 50}))
                    estadoAtual = TELA_MENU;
                break;

            case TELA_SOBRE:
                if (CheckCollisionPointRec(mousePos, (Rectangle){SCREEN_WIDTH / 2 - 100, 580, 200, 50}))
                    estadoAtual = TELA_MENU;
                break;

            case TELA_FIM:
                if (CheckCollisionPointRec(mousePos, (Rectangle){SCREEN_WIDTH / 2 - 100, 450, 200, 50}))
                    estadoAtual = TELA_MENU;
                break;
            }
        }

        if (estadoAtual == TELA_JOGO && turnoAtual == 2)
        {
            WaitTime(1.5);
            processarTurnoIA();
            vencedor = verificarVitoria();
            if (vencedor != 0)
                estadoAtual = TELA_FIM;
        }

        if (estadoAtual == TELA_JOGO && turnoAtual == 1)
        {
            vencedor = verificarVitoria();
            if (vencedor != 0)
                estadoAtual = TELA_FIM;
        }

        BeginDrawing();
        ClearBackground(DARKGREEN);

        switch (estadoAtual)
        {
        case TELA_MENU:
            desenharMenu();
            break;
        case TELA_JOGO:
            desenharTabuleiro();
            desenharMaoHumano();
            desenharMaoIA();
            desenharBotoes();
            desenharMensagem();
            if (turnoAtual == 2)
            {
                DrawRectangle(SCREEN_WIDTH / 2 - 150, SCREEN_HEIGHT - 60, 300, 50, Fade(RED, 0.9f));
                DrawText("TURNO DA IA...", SCREEN_WIDTH / 2 - 90, SCREEN_HEIGHT - 45, 22, WHITE);
            }
            else
            {
                DrawRectangle(SCREEN_WIDTH / 2 - 120, SCREEN_HEIGHT - 60, 240, 50, Fade(GREEN, 0.9f));
                DrawText("SUA VEZ!", SCREEN_WIDTH / 2 - 60, SCREEN_HEIGHT - 45, 22, WHITE);
            }
            break;
        case TELA_REGRAS:
            desenharRegras();
            break;
        case TELA_SOBRE:
            desenharSobre();
            break;
        case TELA_FIM:
            desenharTelaFim();
            break;
        }

        EndDrawing();
    }

    CloseWindow();
    return 0;
}