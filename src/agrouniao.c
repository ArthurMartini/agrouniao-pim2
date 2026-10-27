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
 * Persistencia: PostgreSQL via libpq (compilar com -DUSE_PG). Sem o banco,
 * o programa avisa e usa o arquivo plantios.txt (modo arquivo).
 *
 * Compilar com banco:
 *   gcc -std=c99 -Wall -Wextra -DUSE_PG -I/usr/include/postgresql \
 *       -o agrouniao agrouniao.c -lpq
 * Compilar so com arquivo:
 *   gcc -std=c99 -Wall -Wextra -o agrouniao agrouniao.c
 * Conexao: variavel de ambiente AGRO_DB (padrao "dbname=agrouniao"), ex.:
 *   AGRO_DB="host=localhost dbname=agrouniao user=claude password=..."
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef USE_PG
#include <libpq-fe.h>
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
#ifdef USE_PG
static PGconn *g_db = NULL;

static int dbConectar(void)
{
    const char *ci = getenv("AGRO_DB");
    g_db = PQconnectdb(ci != NULL ? ci : "dbname=agrouniao");
    if (PQstatus(g_db) != CONNECTION_OK) {
        printf("Aviso: sem conexao com o banco.\n  %s", PQerrorMessage(g_db));
        PQfinish(g_db);
        g_db = NULL;
        return 0;
    }
    return 1;
}

static void dbDesconectar(void)
{
    if (g_db != NULL) PQfinish(g_db);
    g_db = NULL;
}

/* Executa SQL parametrizado (sem concatenar texto do usuario no SQL). */
static PGresult *dbExec(const char *sql, int np, const char *const *vals,
                        ExecStatusType esperado)
{
    PGresult *r = PQexecParams(g_db, sql, np, NULL, vals, NULL, NULL, 0);
    if (PQresultStatus(r) != esperado) {
        printf("Erro no banco: %s", PQerrorMessage(g_db));
        PQclear(r);
        return NULL;
    }
    return r;
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
        "LEFT JOIN LATERAL (SELECT volume_previsto FROM previsao_safra "
        "                   WHERE plantio_id = pl.id ORDER BY id DESC LIMIT 1) ps "
        "     ON TRUE "
        "LEFT JOIN safra s ON s.plantio_id = pl.id "
        "ORDER BY pl.id";
    PGresult *r = dbExec(SQL, 0, NULL, PGRES_TUPLES_OK);
    int n = 0, ignorados = 0;
    if (r == NULL) return -1;
    for (int i = 0; i < PQntuples(r); i++) {
        int c = indiceCultura(PQgetvalue(r, i, 2));
        if (c < 0 || n >= MAX_PLANTIOS) { ignorados++; continue; }
        v[n].dbId = atoi(PQgetvalue(r, i, 0));
        snprintf(v[n].produtor, TAM_NOME, "%s", PQgetvalue(r, i, 1));
        v[n].cultura = c;
        v[n].area = (float)atof(PQgetvalue(r, i, 3));
        if (PQgetisnull(r, i, 4) || v[n].area <= 0) {
            v[n].temHistorico = 0;
            v[n].produtividade = 0;
        } else {                       /* produtividade = previsto / area */
            v[n].temHistorico = 1;
            v[n].produtividade = (float)atof(PQgetvalue(r, i, 4)) / v[n].area;
        }
        v[n].colhido = PQgetisnull(r, i, 5) ? -1 : (float)atof(PQgetvalue(r, i, 5));
        n++;
    }
    PQclear(r);
    if (ignorados > 0)
        printf("Aviso: %d plantio(s) ignorado(s) (cultura fora da lista).\n", ignorados);
    return n;
}

/* Grava plantio + previsao de safra numa transacao. */
static int dbInserirPlantio(const Plantio *p)
{
    char idProp[16], idPlantio[16], area[32], prev[32], dias[16];
    const char *v1[1] = { p->produtor };
    const char *v2[4], *v3[2];
    PGresult *r;

    if (!dbExec("BEGIN", 0, NULL, PGRES_COMMAND_OK)) return 0;

    r = dbExec("SELECT pp.id FROM propriedade pp "
               "JOIN produtor pr ON pr.id = pp.produtor_id "
               "WHERE pr.nome = $1 ORDER BY pp.id LIMIT 1",
               1, v1, PGRES_TUPLES_OK);
    if (r == NULL || PQntuples(r) == 0) {
        printf("Produtor sem propriedade cadastrada no banco.\n");
        if (r != NULL) PQclear(r);
        dbExec("ROLLBACK", 0, NULL, PGRES_COMMAND_OK);
        return 0;
    }
    snprintf(idProp, sizeof idProp, "%s", PQgetvalue(r, 0, 0));
    PQclear(r);

    snprintf(area, sizeof area, "%.2f", p->area);
    snprintf(dias, sizeof dias, "%d", DIAS_CICLO);
    v2[0] = NOME_CULTURA[p->cultura]; v2[1] = area; v2[2] = dias; v2[3] = idProp;
    r = dbExec("INSERT INTO plantio (cultura, area_plantada, data_plantio, "
               "data_prev_colheita, propriedade_id) "
               "VALUES ($1, $2, CURRENT_DATE, CURRENT_DATE + $3::int, $4) "
               "RETURNING id", 4, v2, PGRES_TUPLES_OK);
    if (r == NULL) { dbExec("ROLLBACK", 0, NULL, PGRES_COMMAND_OK); return 0; }
    snprintf(idPlantio, sizeof idPlantio, "%s", PQgetvalue(r, 0, 0));
    PQclear(r);

    snprintf(prev, sizeof prev, "%.2f", previsaoSafra(p));
    v3[0] = idPlantio; v3[1] = prev;
    r = dbExec("INSERT INTO previsao_safra (plantio_id, volume_previsto) "
               "VALUES ($1, $2)", 2, v3, PGRES_COMMAND_OK);
    if (r == NULL) { dbExec("ROLLBACK", 0, NULL, PGRES_COMMAND_OK); return 0; }
    PQclear(r);

    r = dbExec("COMMIT", 0, NULL, PGRES_COMMAND_OK);
    if (r == NULL) return 0;
    PQclear(r);
    return 1;
}

/* Registra (ou corrige) a colheita; safra e 1:1 com plantio. */
static int dbRegistrarColheita(int plantioId, float kg)
{
    char id[16], vol[32];
    const char *v[2];
    PGresult *r;
    snprintf(id, sizeof id, "%d", plantioId);
    snprintf(vol, sizeof vol, "%.2f", kg);
    v[0] = id; v[1] = vol;
    r = dbExec("INSERT INTO safra (plantio_id, volume_colhido, data_colheita) "
               "VALUES ($1, $2, CURRENT_DATE) "
               "ON CONFLICT (plantio_id) DO UPDATE SET "
               "volume_colhido = EXCLUDED.volume_colhido, "
               "data_colheita = EXCLUDED.data_colheita",
               2, v, PGRES_COMMAND_OK);
    if (r == NULL) return 0;
    PQclear(r);
    return 1;
}
#else
/* Sem -DUSE_PG: apenas modo arquivo. */
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
           online ? "CONECTADO (PostgreSQL)" : "OFFLINE - usando arquivo");
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
