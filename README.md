# Domino Clash

Jogo de dominó desenvolvido em C com interface gráfica e inteligência artificial.

## Descrição

Jogo de dominó tradicional onde você joga contra uma IA baseada em modelo de linguagem. O objetivo é ficar sem peças antes do oponente.

## Como executar

Execute o arquivo `JOGAR.bat` para rodar o jogo.

## Como jogar

1. Selecione uma peça clicando nela
2. Escolha o lado (esquerda ou direita)
3. Clique em "JOGAR"
4. Se não tiver jogada válida, clique em "COMPRAR"

## Estruturas de dados utilizadas

### Lista Duplamente Encadeada
- **Uso**: Tabuleiro do jogo
- **Vantagem**: Permite inserção de peças tanto na extremidade esquerda quanto direita
- **Implementação**: Cada nó possui ponteiros para próximo e anterior

### Lista Encadeada Simples
- **Uso**: Mão dos jogadores
- **Vantagem**: Inserção e remoção dinâmica de peças
- **Implementação**: Cada nó aponta apenas para o próximo

### Pilha LIFO (Last In, First Out)
- **Uso**: Monte de compras
- **Vantagem**: Última peça adicionada é a primeira a ser comprada
- **Implementação**: Array estático com índice de topo

### Algoritmo Bubble Sort
- **Uso**: Ordenação das peças por valor
- **Complexidade**: O(n²)
- **Critério**: Peças com maior soma (lado1 + lado2) ficam primeiro

## Tecnologias e Bibliotecas

### Linguagem
- **C (C99)** - Linguagem de programação principal

### Bibliotecas de Interface Gráfica
- **Raylib 5.0** - Biblioteca multiplataforma para jogos 2D/3D
  - Renderização de janelas e gráficos
  - Tratamento de eventos (mouse, teclado)
  - Desenho de formas e textos

### Bibliotecas de Rede e Dados
- **libcurl** - Biblioteca para requisições HTTP/HTTPS
  - Comunicação com a API Groq
  - Suporte a HTTPS com autenticação via Bearer token

- **cJSON** - Biblioteca para parsing de JSON
  - Construção de payloads para API
  - Parsing de respostas da IA

### Inteligência Artificial
- **Groq API** - Plataforma de inferência de IA em nuvem
  - Endpoint: `https://api.groq.com/openai/v1/chat/completions`
  - Protocolo: REST API compatível com OpenAI

- **Modelo: Llama 3.3 70B Versatile**
  - 70 bilhões de parâmetros
  - Modelo de linguagem de propósito geral
  - Analisa o estado do jogo e decide a melhor jogada

- **Sistema de Fallback**: IA local simples caso a API não responda

## Arquitetura do Projeto

```
main.c              # Código principal do jogo
├── Estruturas
│   ├── Peca       # Representa uma peça de dominó
│   ├── Tabuleiro  # Lista duplamente encadeada
│   ├── Mao        # Lista encadeada simples
│   └── Monte      # Pilha LIFO
├── Funções de Estruturas
│   ├── inicializar()
│   ├── adicionarPeca()
│   ├── removerPeca()
│   ├── ordenarMao()     # Bubble Sort
│   └── inserir()
├── Inteligência Artificial
│   ├── construirPromptIA()  # Monta contexto do jogo
│   ├── chamarGroqAPI()      # Requisição HTTP
│   └── parseResposta()      # Parse JSON
└── Interface Gráfica
    ├── desenharMenu()
    ├── desenharJogo()
    ├── desenharRegras()
    └── desenharSobre()
```

## Compilação

```bash
gcc main.c -o domino_clash.exe -Iinclude -Llib \
    lib/libraylib.a lib/libcjson.a lib/libcurl.dll.a \
    -lopengl32 -lgdi32 -lwinmm -lws2_32
```

## Autores

- Davi Santiago
- José Jorge
