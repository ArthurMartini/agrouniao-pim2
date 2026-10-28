/*
 * AgroUniao - Previsao de safra e pagamento proporcional
 * Etapa 5 do PIM II - Programacao Estruturada em C
 *
 * Implementa tres requisitos do sistema:
 *   RF03 - previsao de safra por plantio
 *   RF05 - consolidacao da producao por cultura
 *   RF08 - pagamento proporcional por cooperado (RN02)
 *
 * Acesso: login com dois perfis (Produtor e Gestor), cada um com seu menu.
 * Persistencia: SQL Server via ODBC (compilar com -DUSE_DB). Sem o banco,
 * o programa avisa e usa o arquivo plantios.txt (modo arquivo).
 *
 * Compilar com banco (Windows, MinGW-w64):
 *   gcc -std=c99 -Wall -Wextra -DUSE_DB -o agrouniao agrouniao.c -lodbc32
 * Compilar com banco (Linux, unixODBC):
 *   gcc -std=c99 -Wall -Wextra -DUSE_DB -o agrouniao agrouniao.c -lodbc
 * Compilar so com arquivo:
 *   gcc -std=c99 -Wall -Wextra -o agrouniao agrouniao.c
 * Conexao: variavel de ambiente AGRO_DB com a connection string ODBC
 * (padrao: SQL Server local, autenticacao do Windows, banco agrouniao), ex.:
 *   AGRO_DB="Driver={ODBC Driver 17 for SQL Server};Server=localhost;
 *           Database=agrouniao;UID=usuario;PWD=senha;TrustServerCertificate=yes;"
 *   (digitar tudo em uma unica linha)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef USE_DB
#include <stdint.h>
#ifdef _WIN32
#include <windows.h>
#endif
#include <sql.h>
#include <sqlext.h>
#endif

#define MAX_PLANTIOS 100
#define MAX_CULTURAS 5
#define ANOS_HIST    3
#define TAM_NOME     40
#define ARQ_DADOS    "plantios.txt"
#define ARQ_REL      "relatorio.txt"
#define DIAS_CICLO   90      /* dias entre plantio e colheita prevista */

typedef struct {
    char  produtor[TAM_NOME];
    int   cultura;          /* indice em NOME_CULTURA */
    float area;             /* hectares */
    int   temHistorico;     /* 1 se o produtor informou produtividade propria */
    float produtividade;    /* kg por hectare (valida se temHistorico == 1) */
    float colhido;          /* kg colhidos; -1 enquanto nao houver colheita */
    int   dbId;             /* plantio.id no banco; 0 se nao veio do banco */
} Plantio;

typedef enum { PERFIL_PRODUTOR = 1, PERFIL_GESTOR = 2 } Perfil;

typedef struct {
    const char *login;
    const char *senha;
    Perfil      perfil;
    const char *produtor;   /* nome do produtor (so no perfil Produtor) */
} Usuario;

/* Autenticacao simplificada, apenas para fins didaticos (senha em texto). */
static const Usuario USUARIOS[] = {
    { "produtor", "agro123", PERFIL_PRODUTOR, "Produtor A" },
    { "gestor",   "coop123", PERFIL_GESTOR,   NULL }
};
#define QTD_USUARIOS ((int)(sizeof USUARIOS / sizeof USUARIOS[0]))

static const char *NOME_CULTURA[MAX_CULTURAS] =
    { "Tomate", "Alface", "Cenoura", "Milho", "Feijao" };

/* Matriz cultura x ano: produtividade regional em kg/ha.
 * Valores ficticios, apenas para demonstracao do algoritmo. */
static const float HISTORICO_REGIONAL[MAX_CULTURAS][ANOS_HIST] = {
    { 48000, 52000, 50000 },   /* Tomate  */
    { 22000, 24000, 23000 },   /* Alface  */
    { 35000, 37000, 36000 },   /* Cenoura */
    {  5800,  6200,  6000 },   /* Milho   */
    {  2300,  2500,  2400 }    /* Feijao  */
};

/* ---------------------------------------------------------- entrada */
static void lerTexto(const char *msg, char *dest, int tam)
{
    do {
        printf("%s", msg);
        if (fgets(dest, tam, stdin) == NULL) { dest[0] = '\0'; return; }
        dest[strcspn(dest, "\n")] = '\0';
    } while (dest[0] == '\0');
}

static float lerFloat(const char *msg, float minimo, int aceitaIgual)
{
    char buf[64], *fim;
    float v;
    for (;;) {
        printf("%s", msg);
        if (fgets(buf, sizeof buf, stdin) == NULL) return minimo;
        for (char *c = buf; *c; c++)          /* aceita virgula decimal */
            if (*c == ',') *c = '.';
        v = strtof(buf, &fim);
        if (fim != buf && *fim == '\n' &&
            (v > minimo || (aceitaIgual && v == minimo)))
            return v;
        printf("  Valor invalido. Tente novamente.\n");
    }
}

static int lerInteiro(const char *msg, int min, int max)
{
    char buf[64], *fim;
    long v;
    for (;;) {
        printf("%s", msg);
        if (fgets(buf, sizeof buf, stdin) == NULL) return min;
        v = strtol(buf, &fim, 10);
        if (fim != buf && *fim == '\n' && v >= min && v <= max) return (int)v;
        printf("  Digite um numero entre %d e %d.\n", min, max);
    }
}

/* ---------------------------------------------------------- regras */
static float produtividadeRegional(int cultura)
{
    float soma = 0;
    for (int a = 0; a < ANOS_HIST; a++)
        soma += HISTORICO_REGIONAL[cultura][a];
    return soma / ANOS_HIST;
}

static float previsaoSafra(const Plantio *p)
{
    float prod;
    if (p->temHistorico)
        prod = p->produtividade;
    else
        prod = produtividadeRegional(p->cultura);
    return p->area * prod;
}

static void consolidarPorCultura(const Plantio v[], int n, float total[])
{
    for (int c = 0; c < MAX_CULTURAS; c++) total[c] = 0;
    for (int i = 0; i < n; i++)
        total[v[i].cultura] += previsaoSafra(&v[i]);
}

static int indiceProdutor(char nomes[][TAM_NOME], int qtd, const char *nome)
{
    for (int i = 0; i < qtd; i++)
        if (strcmp(nomes[i], nome) == 0) return i;
    return -1;
}

/* Soma o volume colhido por produtor. Retorna a quantidade de produtores. */
static int volumePorProdutor(const Plantio v[], int n,
                             char nomes[][TAM_NOME], float volume[])
{
    int qtd = 0;
    for (int i = 0; i < n; i++) {
        if (v[i].colhido < 0) continue;       /* ainda nao colhido */
        int k = indiceProdutor(nomes, qtd, v[i].produtor);
        if (k < 0) {
            k = qtd++;
            strcpy(nomes[k], v[i].produtor);
            volume[k] = 0;
        }
        volume[k] += v[i].colhido;
    }
    return qtd;
}

/* ---------------------------------------------------------- arquivos */
static int carregar(Plantio v[])
{
    FILE *f = fopen(ARQ_DADOS, "r");
    int n = 0;
    if (f == NULL) return 0;                  /* primeira execucao */
    while (n < MAX_PLANTIOS &&
           fscanf(f, " %39[^;];%d;%f;%d;%f;%f", v[n].produtor, &v[n].cultura,
                  &v[n].area, &v[n].temHistorico, &v[n].produtividade,
                  &v[n].colhido) == 6) {
        v[n].dbId = 0;
        n++;
    }
    fclose(f);
    return n;
}

static int salvar(const Plantio v[], int n)
{
    FILE *f = fopen(ARQ_DADOS, "w");
    if (f == NULL) return 0;
    for (int i = 0; i < n; i++)
        fprintf(f, "%s;%d;%.2f;%d;%.2f;%.2f\n", v[i].produtor, v[i].cultura,
                v[i].area, v[i].temHistorico, v[i].produtividade, v[i].colhido);
    fclose(f);
    return 1;
}


/* ---------------------------------------------------------- banco */
#ifdef USE_DB
#define TAM_CAMPO 128

static SQLHENV g_env = SQL_NULL_HENV;
static SQLHDBC g_dbc = SQL_NULL_HDBC;

static void dbMostrarErro(SQLSMALLINT tipo, SQLHANDLE h)
{
    SQLCHAR estado[8], msg[512];
    SQLINTEGER nativo;
    SQLSMALLINT tam;
    if (SQL_SUCCEEDED(SQLGetDiagRec(tipo, h, 1, estado, &nativo,
                                    msg, (SQLSMALLINT)sizeof msg, &tam)))
        printf("  %s\n", (char *)msg);
}

static int dbConectar(void)
{
    const char *ci = getenv("AGRO_DB");
    SQLCHAR saida[512];
    SQLSMALLINT tam;
    SQLRETURN rc;
    if (ci == NULL)
        ci = "Driver={ODBC Driver 17 for SQL Server};Server=localhost;"
             "Database=agrouniao;Trusted_Connection=yes;"
             "TrustServerCertificate=yes;";
    if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &g_env)))
        return 0;
    SQLSetEnvAttr(g_env, SQL_ATTR_ODBC_VERSION,
                  (SQLPOINTER)(intptr_t)SQL_OV_ODBC3, 0);
    SQLAllocHandle(SQL_HANDLE_DBC, g_env, &g_dbc);
    rc = SQLDriverConnect(g_dbc, NULL, (SQLCHAR *)ci, SQL_NTS, saida,
                          (SQLSMALLINT)sizeof saida, &tam, SQL_DRIVER_NOPROMPT);
    if (!SQL_SUCCEEDED(rc)) {
        printf("Aviso: sem conexao com o banco.\n");
        dbMostrarErro(SQL_HANDLE_DBC, g_dbc);
        SQLFreeHandle(SQL_HANDLE_DBC, g_dbc);
        SQLFreeHandle(SQL_HANDLE_ENV, g_env);
        g_dbc = SQL_NULL_HDBC;
        g_env = SQL_NULL_HENV;
        return 0;
    }
    return 1;
}

static void dbDesconectar(void)
{
    if (g_dbc != SQL_NULL_HDBC) {
        SQLDisconnect(g_dbc);
        SQLFreeHandle(SQL_HANDLE_DBC, g_dbc);
    }
    if (g_env != SQL_NULL_HENV) SQLFreeHandle(SQL_HANDLE_ENV, g_env);
    g_dbc = SQL_NULL_HDBC;
    g_env = SQL_NULL_HENV;
}

/* Executa SQL parametrizado (marcadores ?), sem concatenar texto do usuario
 * no SQL. Devolve o statement (quem chamou libera) ou NULL em caso de erro. */
static SQLHSTMT dbExec(const char *sql, int np, const char *const *vals)
{
    SQLHSTMT st = SQL_NULL_HSTMT;
    if (!SQL_SUCCEEDED(SQLAllocHandle(SQL_HANDLE_STMT, g_dbc, &st))) return NULL;
    if (SQL_SUCCEEDED(SQLPrepare(st, (SQLCHAR *)sql, SQL_NTS))) {
        for (int i = 0; i < np; i++) {
            SQLULEN tam = strlen(vals[i]) > 0 ? strlen(vals[i]) : 1;
            SQLBindParameter(st, (SQLUSMALLINT)(i + 1), SQL_PARAM_INPUT, SQL_C_CHAR,
                             SQL_VARCHAR, tam, 0, (SQLPOINTER)vals[i], 0, NULL);
        }
        if (SQL_SUCCEEDED(SQLExecute(st))) return st;
    }
    printf("Erro no banco:\n");
    dbMostrarErro(SQL_HANDLE_STMT, st);
    SQLFreeHandle(SQL_HANDLE_STMT, st);
    return NULL;
}

/* Le a coluna 'col' (1..n) como texto. Retorna 0 se o valor for NULL. */
static int dbCampo(SQLHSTMT st, int col, char *dest, int tam)
{
    SQLLEN ind = 0;
    dest[0] = '\0';
    if (!SQL_SUCCEEDED(SQLGetData(st, (SQLUSMALLINT)col, SQL_C_CHAR,
                                  dest, tam, &ind)))
        return 0;
    return ind != SQL_NULL_DATA;
}

/* ligada = 0: abre uma transacao; ligada = 1: volta ao autocommit. */
static void dbTransacao(int ligada)
{
    intptr_t modo = ligada ? SQL_AUTOCOMMIT_ON : SQL_AUTOCOMMIT_OFF;
    SQLSetConnectAttr(g_dbc, SQL_ATTR_AUTOCOMMIT, (SQLPOINTER)modo, 0);
}

static int dbFim(int confirmar)
{
    SQLRETURN rc = SQLEndTran(SQL_HANDLE_DBC, g_dbc,
                              confirmar ? SQL_COMMIT : SQL_ROLLBACK);
    dbTransacao(1);
    return SQL_SUCCEEDED(rc);
}

static int indiceCultura(const char *nome)
{
    for (int c = 0; c < MAX_CULTURAS; c++)
        if (strcmp(NOME_CULTURA[c], nome) == 0) return c;
    return -1;
}

/* Le os plantios do banco. Retorna a quantidade, ou -1 em caso de erro. */
static int dbCarregar(Plantio v[])
{
    static const char *SQL =
        "SELECT pl.id, pr.nome, pl.cultura, pl.area_plantada, "
        "       ps.volume_previsto, s.volume_colhido "
        "FROM plantio pl "
        "JOIN propriedade pp ON pp.id = pl.propriedade_id "
        "JOIN produtor pr ON pr.id = pp.produtor_id "
        "OUTER APPLY (SELECT TOP 1 volume_previsto FROM previsao_safra "
        "             WHERE plantio_id = pl.id ORDER BY id DESC) ps "
        "LEFT JOIN safra s ON s.plantio_id = pl.id "
        "ORDER BY pl.id";
    SQLHSTMT st = dbExec(SQL, 0, NULL);
    char id[TAM_CAMPO], nome[TAM_NOME], cultura[TAM_CAMPO];
    char area[TAM_CAMPO], prev[TAM_CAMPO], colhido[TAM_CAMPO];
    int n = 0, ignorados = 0;
    if (st == NULL) return -1;
    while (SQL_SUCCEEDED(SQLFetch(st))) {
        int c, temPrev, temColhido;
        dbCampo(st, 1, id, sizeof id);
        dbCampo(st, 2, nome, sizeof nome);
        dbCampo(st, 3, cultura, sizeof cultura);
        dbCampo(st, 4, area, sizeof area);
        temPrev = dbCampo(st, 5, prev, sizeof prev);
        temColhido = dbCampo(st, 6, colhido, sizeof colhido);
        c = indiceCultura(cultura);
        if (c < 0 || n >= MAX_PLANTIOS) { ignorados++; continue; }
        v[n].dbId = atoi(id);
        snprintf(v[n].produtor, TAM_NOME, "%s", nome);
        v[n].cultura = c;
        v[n].area = (float)atof(area);
        if (!temPrev || v[n].area <= 0) {
            v[n].temHistorico = 0;
            v[n].produtividade = 0;
        } else {                       /* produtividade = previsto / area */
            v[n].temHistorico = 1;
            v[n].produtividade = (float)atof(prev) / v[n].area;
        }
        v[n].colhido = temColhido ? (float)atof(colhido) : -1;
        n++;
    }
    SQLFreeHandle(SQL_HANDLE_STMT, st);
    if (ignorados > 0)
        printf("Aviso: %d plantio(s) ignorado(s) (cultura fora da lista).\n", ignorados);
    return n;
}

/* Grava plantio + previsao de safra numa transacao. */
static int dbInserirPlantio(const Plantio *p)
{
    char idProp[TAM_CAMPO], idPlantio[TAM_CAMPO], area[32], prev[32], dias[16];
    const char *v1[1] = { p->produtor };
    const char *v2[4], *v3[2];
    SQLHSTMT st;

    dbTransacao(0);

    st = dbExec("SELECT TOP 1 pp.id FROM propriedade pp "
                "JOIN produtor pr ON pr.id = pp.produtor_id "
                "WHERE pr.nome = ? ORDER BY pp.id",
                1, v1);
    if (st == NULL) { dbFim(0); return 0; }
    if (!SQL_SUCCEEDED(SQLFetch(st))) {
        printf("Produtor sem propriedade cadastrada no banco.\n");
        SQLFreeHandle(SQL_HANDLE_STMT, st);
        dbFim(0);
        return 0;
    }
    dbCampo(st, 1, idProp, sizeof idProp);
    SQLFreeHandle(SQL_HANDLE_STMT, st);

    snprintf(area, sizeof area, "%.2f", p->area);
    snprintf(dias, sizeof dias, "%d", DIAS_CICLO);
    v2[0] = NOME_CULTURA[p->cultura]; v2[1] = area; v2[2] = dias; v2[3] = idProp;
    st = dbExec("INSERT INTO plantio (cultura, area_plantada, data_plantio, "
                "data_prev_colheita, propriedade_id) "
                "OUTPUT INSERTED.id "
                "VALUES (?, CAST(? AS NUMERIC(10,2)), CAST(GETDATE() AS DATE), "
                "DATEADD(DAY, CAST(? AS INT), CAST(GETDATE() AS DATE)), "
                "CAST(? AS INT))",
                4, v2);
    if (st == NULL) { dbFim(0); return 0; }
    if (!SQL_SUCCEEDED(SQLFetch(st))) {
        SQLFreeHandle(SQL_HANDLE_STMT, st);
        dbFim(0);
        return 0;
    }
    dbCampo(st, 1, idPlantio, sizeof idPlantio);
    SQLFreeHandle(SQL_HANDLE_STMT, st);

    snprintf(prev, sizeof prev, "%.2f", previsaoSafra(p));
    v3[0] = idPlantio; v3[1] = prev;
    st = dbExec("INSERT INTO previsao_safra (plantio_id, volume_previsto) "
                "VALUES (CAST(? AS INT), CAST(? AS NUMERIC(12,2)))", 2, v3);
    if (st == NULL) { dbFim(0); return 0; }
    SQLFreeHandle(SQL_HANDLE_STMT, st);

    return dbFim(1);
}

/* Registra (ou corrige) a colheita; safra e 1:1 com plantio.
 * Tenta UPDATE; se nenhuma linha mudou, faz o INSERT. */
static int dbRegistrarColheita(int plantioId, float kg)
{
    char id[16], vol[32];
    const char *v[2];
    SQLHSTMT st;
    SQLLEN linhas = 0;
    snprintf(id, sizeof id, "%d", plantioId);
    snprintf(vol, sizeof vol, "%.2f", kg);

    v[0] = vol; v[1] = id;
    st = dbExec("UPDATE safra SET volume_colhido = CAST(? AS NUMERIC(12,2)), "
                "data_colheita = CAST(GETDATE() AS DATE) "
                "WHERE plantio_id = CAST(? AS INT)", 2, v);
    if (st == NULL) return 0;
    SQLRowCount(st, &linhas);
    SQLFreeHandle(SQL_HANDLE_STMT, st);
    if (linhas > 0) return 1;

    v[0] = id; v[1] = vol;
    st = dbExec("INSERT INTO safra (plantio_id, volume_colhido, data_colheita) "
                "VALUES (CAST(? AS INT), CAST(? AS NUMERIC(12,2)), "
                "CAST(GETDATE() AS DATE))", 2, v);
    if (st == NULL) return 0;
    SQLFreeHandle(SQL_HANDLE_STMT, st);
    return 1;
}
#else
/* Sem -DUSE_DB: apenas modo arquivo. */
static int dbConectar(void) { return 0; }
static void dbDesconectar(void) { }
static int dbCarregar(Plantio v[]) { (void)v; return -1; }
static int dbInserirPlantio(const Plantio *p) { (void)p; return 0; }
static int dbRegistrarColheita(int id, float kg) { (void)id; (void)kg; return 0; }
#endif

/* ---------------------------------------------------------- saidas */
/* dono != NULL: lista apenas os plantios desse produtor (N = indice global). */
static void imprimirPlantios(const Plantio v[], int n, const char *dono)
{
    printf("\n %-3s %-12s %-8s %8s %12s %12s\n", "N", "Produtor", "Cultura",
           "Area(ha)", "Previsao(kg)", "Colhido(kg)");
    for (int i = 0; i < n; i++) {
        if (dono != NULL && strcmp(v[i].produtor, dono) != 0) continue;
        printf(" %-3d %-12s %-8s %8.2f %12.2f ", i + 1, v[i].produtor,
               NOME_CULTURA[v[i].cultura], v[i].area, previsaoSafra(&v[i]));
        if (v[i].colhido < 0) printf("%12s\n", "-");
        else                  printf("%12.2f\n", v[i].colhido);
    }
}

static void imprimirConsolidado(FILE *out, const float total[])
{
    fprintf(out, "\nProducao prevista por cultura\n");
    for (int c = 0; c < MAX_CULTURAS; c++)
        if (total[c] > 0)
            fprintf(out, "  %-8s %12.2f kg\n", NOME_CULTURA[c], total[c]);
}

static void imprimirPagamentos(FILE *out, const Plantio v[], int n, float preco)
{
    char  nomes[MAX_PLANTIOS][TAM_NOME];
    float volume[MAX_PLANTIOS], totalVol = 0;
    int   qtd = volumePorProdutor(v, n, nomes, volume);

    for (int k = 0; k < qtd; k++) totalVol += volume[k];
    fprintf(out, "\nPagamento proporcional (preco %.2f por kg)\n", preco);
    if (qtd == 0) { fprintf(out, "  Nenhuma colheita registrada.\n"); return; }
    for (int k = 0; k < qtd; k++)
        fprintf(out, "  %-12s %10.2f kg (%5.1f%%)  R$ %12.2f\n", nomes[k],
                volume[k], 100.0f * volume[k] / totalVol, volume[k] * preco);
    fprintf(out, "  Total        %10.2f kg           R$ %12.2f\n",
            totalVol, totalVol * preco);
}

static void gerarRelatorio(const Plantio v[], int n, float preco)
{
    float total[MAX_CULTURAS];
    FILE *f = fopen(ARQ_REL, "w");
    if (f == NULL) { printf("Erro ao criar %s\n", ARQ_REL); return; }
    consolidarPorCultura(v, n, total);
    fprintf(f, "RELATORIO AGROUNIAO\n");
    imprimirConsolidado(f, total);
    if (preco > 0) imprimirPagamentos(f, v, n, preco);
    fclose(f);
    printf("Relatorio gravado em %s\n", ARQ_REL);
}

/* ---------------------------------------------------------- acesso */
static const Usuario *login(void)
{
    char l[TAM_NOME], s[TAM_NOME];
    for (int t = 1; t <= 3; t++) {
        lerTexto("Login: ", l, TAM_NOME);
        lerTexto("Senha: ", s, TAM_NOME);
        for (int i = 0; i < QTD_USUARIOS; i++)
            if (strcmp(l, USUARIOS[i].login) == 0 &&
                strcmp(s, USUARIOS[i].senha) == 0)
                return &USUARIOS[i];
        printf("  Login ou senha invalidos (%d/3).\n", t);
    }
    return NULL;
}

/* ---------------------------------------------------------- menu */
/* online: dados no banco; n e recarregado apos cada gravacao. */
static void atualizar(Plantio v[], int *n, int online)
{
    if (online) {
        int k = dbCarregar(v);
        if (k >= 0) *n = k;
    }
}

static void cadastrarPlantio(Plantio v[], int *n, const char *produtor, int online)
{
    Plantio novo;
    if (*n >= MAX_PLANTIOS) { printf("Limite de plantios atingido.\n"); return; }
    snprintf(novo.produtor, TAM_NOME, "%s", produtor);
    printf("Produtor: %s\n", novo.produtor);
    printf("Culturas: ");
    for (int c = 0; c < MAX_CULTURAS; c++) printf("%d-%s ", c + 1, NOME_CULTURA[c]);
    printf("\n");
    novo.cultura = lerInteiro("Cultura: ", 1, MAX_CULTURAS) - 1;
    novo.area    = lerFloat("Area plantada (ha): ", 0, 0);
    novo.temHistorico = lerInteiro("Tem produtividade propria? (1-sim 0-nao): ", 0, 1);
    novo.produtividade = 0;
    if (novo.temHistorico)
        novo.produtividade = lerFloat("Produtividade (kg/ha): ", 0, 0);
    novo.colhido = -1;
    novo.dbId = 0;

    if (online) {
        if (!dbInserirPlantio(&novo)) { printf("Plantio NAO cadastrado.\n"); return; }
        atualizar(v, n, online);
        printf("Plantio gravado no banco. Previsao: %.2f kg\n", previsaoSafra(&novo));
    } else {
        v[(*n)++] = novo;
        printf("Plantio cadastrado (arquivo). Previsao: %.2f kg\n", previsaoSafra(&novo));
    }
}

static void registrarColheita(Plantio v[], int *n, const char *dono, int online)
{
    if (*n == 0) { printf("Nenhum plantio cadastrado.\n"); return; }
    imprimirPlantios(v, *n, dono);
    int i = lerInteiro("Numero do plantio: ", 1, *n) - 1;
    if (dono != NULL && strcmp(v[i].produtor, dono) != 0) {
        printf("Esse plantio pertence a outro produtor.\n");
        return;
    }
    float kg = lerFloat("Volume colhido (kg): ", 0, 1);
    if (online) {
        if (!dbRegistrarColheita(v[i].dbId, kg)) { printf("Colheita NAO registrada.\n"); return; }
        atualizar(v, n, online);
        printf("Colheita gravada no banco.\n");
    } else {
        v[i].colhido = kg;
        printf("Colheita registrada (arquivo).\n");
    }
}

static void menuProdutor(const Usuario *u, Plantio v[], int *n, int online)
{
    int opcao;
    do {
        atualizar(v, n, online);
        printf("\n=== AgroUniao | Produtor: %s ===\n1-Cadastrar plantio\n"
               "2-Meus plantios\n3-Registrar colheita\n0-Sair\n", u->produtor);
        opcao = lerInteiro("Opcao: ", 0, 3);
        switch (opcao) {
            case 1: cadastrarPlantio(v, n, u->produtor, online); break;
            case 2: imprimirPlantios(v, *n, u->produtor); break;
            case 3: registrarColheita(v, n, u->produtor, online); break;
            default: break;
        }
    } while (opcao != 0);
}

static void menuGestor(Plantio v[], int *n, int online)
{
    float total[MAX_CULTURAS], preco = 0;
    int opcao;
    do {
        atualizar(v, n, online);
        printf("\n=== AgroUniao | Gestor da cooperativa ===\n1-Listar plantios\n"
               "2-Previsao por cultura\n3-Pagamento proporcional\n"
               "4-Gerar relatorio\n0-Sair\n");
        opcao = lerInteiro("Opcao: ", 0, 4);
        switch (opcao) {
            case 1: imprimirPlantios(v, *n, NULL); break;
            case 2:
                consolidarPorCultura(v, *n, total);
                imprimirConsolidado(stdout, total);
                break;
            case 3:
                preco = lerFloat("Preco por kg (R$): ", 0, 0);
                imprimirPagamentos(stdout, v, *n, preco);
                break;
            case 4: gerarRelatorio(v, *n, preco); break;
            default: break;
        }
    } while (opcao != 0);
}

int main(void)
{
    Plantio plantios[MAX_PLANTIOS];
    int online = dbConectar();
    int n = 0;
    const Usuario *u;

    printf("=== AgroUniao ===\nBanco de dados: %s\n\n",
           online ? "CONECTADO (SQL Server)" : "OFFLINE - usando arquivo");
    u = login();
    if (u == NULL) { printf("Acesso negado.\n"); dbDesconectar(); return 1; }

    if (online) {
        n = dbCarregar(plantios);
        if (n < 0) { n = 0; online = 0; printf("Falha ao ler o banco; modo arquivo.\n"); }
    } else {
        n = carregar(plantios);
    }

    if (u->perfil == PERFIL_PRODUTOR) menuProdutor(u, plantios, &n, online);
    else                              menuGestor(plantios, &n, online);

    if (!online && !salvar(plantios, n))
        printf("Aviso: nao foi possivel salvar os dados.\n");
    dbDesconectar();
    return 0;
}
