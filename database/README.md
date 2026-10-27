# Banco de dados (PostgreSQL 16)

Modelo da Etapa 4 do PIM II. O script `agrouniao_schema.sql` cria as tabelas e carrega dados de exemplo.

```bash
createdb agrouniao
psql -d agrouniao -f database/agrouniao_schema.sql
```

| Tabela | Papel |
|---|---|
| `cooperativa` | Dados da cooperativa (nome, CNPJ) |
| `produtor` | Cooperados; pertencem a uma cooperativa |
| `propriedade` | Propriedades de cada produtor (área, localização) |
| `plantio` | Cultura, área plantada e datas de plantio e colheita prevista |
| `previsao_safra` | Volume previsto para cada plantio |
| `safra` | Volume colhido, em relação 1:1 com o plantio |
| `leitura_sensor` | Umidade e temperatura por propriedade, com data e hora |
| `comprador` | Compradores da produção |
| `negociacao` | Venda coletiva: volume, preço e comprador |
| `pagamento` | Valor pago a cada produtor por negociação |

Arquivos:
- `agrouniao_schema.sql`: tabelas, restrições e dados de exemplo.
- `agrouniao_consultas.sql`: consultas de produção, consolidação e pagamento proporcional.
- `demo_select.sql`: SELECT usado para conferir o que o programa em C gravou.

Etapa 4 (Modelagem de Banco de Dados), responsável: Bruno.
