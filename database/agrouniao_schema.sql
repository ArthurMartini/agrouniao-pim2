-- AgroUnião — modelo físico (SQL Server 2019 ou superior, T-SQL)
-- Etapa 4 — Modelagem de Banco de Dados
-- Executar no SSMS ou com: sqlcmd -S localhost -E -i agrouniao_schema.sql

IF DB_ID(N'agrouniao') IS NULL CREATE DATABASE agrouniao;
GO
USE agrouniao;
GO

CREATE TABLE cooperativa (
    id    INT IDENTITY(1,1) PRIMARY KEY,
    nome  VARCHAR(150) NOT NULL,
    cnpj  VARCHAR(18)  NOT NULL UNIQUE
);

CREATE TABLE produtor (
    id              INT IDENTITY(1,1) PRIMARY KEY,
    nome            VARCHAR(150) NOT NULL,
    cpf             VARCHAR(14)  NOT NULL UNIQUE,
    cooperativa_id  INT NOT NULL REFERENCES cooperativa(id)
);

CREATE TABLE propriedade (
    id           INT IDENTITY(1,1) PRIMARY KEY,
    nome         VARCHAR(150) NOT NULL,
    area         NUMERIC(10,2) NOT NULL CHECK (area > 0),
    localizacao  VARCHAR(255),
    produtor_id  INT NOT NULL REFERENCES produtor(id)
);

CREATE TABLE plantio (
    id                   INT IDENTITY(1,1) PRIMARY KEY,
    cultura              VARCHAR(80)   NOT NULL,
    area_plantada        NUMERIC(10,2) NOT NULL CHECK (area_plantada > 0),
    data_plantio         DATE NOT NULL,
    data_prev_colheita   DATE NOT NULL,
    propriedade_id       INT NOT NULL REFERENCES propriedade(id),
    CHECK (data_prev_colheita > data_plantio)
);

CREATE TABLE previsao_safra (
    id                INT IDENTITY(1,1) PRIMARY KEY,
    plantio_id        INT NOT NULL REFERENCES plantio(id),
    volume_previsto   NUMERIC(12,2) NOT NULL CHECK (volume_previsto >= 0),
    data_calculo      DATE NOT NULL DEFAULT CAST(GETDATE() AS DATE)
);

CREATE TABLE safra (
    id              INT IDENTITY(1,1) PRIMARY KEY,
    plantio_id      INT NOT NULL,
    volume_colhido  NUMERIC(12,2) NOT NULL CHECK (volume_colhido >= 0),
    data_colheita   DATE NOT NULL,
    CONSTRAINT uq_safra_plantio UNIQUE (plantio_id),   -- relação 1:1
    CONSTRAINT fk_safra_plantio FOREIGN KEY (plantio_id) REFERENCES plantio(id)
);

CREATE TABLE leitura_sensor (
    id              BIGINT IDENTITY(1,1) PRIMARY KEY,
    propriedade_id  INT NOT NULL REFERENCES propriedade(id),
    umidade         NUMERIC(5,2) CHECK (umidade BETWEEN 0 AND 100),
    temperatura     NUMERIC(5,2),
    data_hora       DATETIME2(0) NOT NULL DEFAULT SYSDATETIME()
);
CREATE INDEX idx_leitura_prop_data ON leitura_sensor (propriedade_id, data_hora DESC);

CREATE TABLE comprador (
    id    INT IDENTITY(1,1) PRIMARY KEY,
    nome  VARCHAR(150) NOT NULL,
    tipo  VARCHAR(40)  NOT NULL
);

CREATE TABLE negociacao (
    id              INT IDENTITY(1,1) PRIMARY KEY,
    volume          NUMERIC(12,2) NOT NULL CHECK (volume > 0),
    preco           NUMERIC(12,2) NOT NULL CHECK (preco > 0),   -- preço por unidade
    data            DATE NOT NULL,
    comprador_id    INT NOT NULL REFERENCES comprador(id),
    cooperativa_id  INT NOT NULL REFERENCES cooperativa(id)
);

CREATE TABLE pagamento (
    id              INT IDENTITY(1,1) PRIMARY KEY,
    valor           NUMERIC(12,2) NOT NULL CHECK (valor >= 0),
    data            DATE NOT NULL,
    produtor_id     INT NOT NULL REFERENCES produtor(id),
    negociacao_id   INT NOT NULL REFERENCES negociacao(id)
);

GO

-- ---------------------------------------------------------------
-- Dados de exemplo (apenas para validar o modelo)
-- ---------------------------------------------------------------
INSERT INTO cooperativa (nome, cnpj) VALUES (N'Cooperativa AgroUnião', '12.345.678/0001-90');

INSERT INTO produtor (nome, cpf, cooperativa_id) VALUES
 ('Produtor A', '111.111.111-11', 1),
 ('Produtor B', '222.222.222-22', 1);

INSERT INTO propriedade (nome, area, localizacao, produtor_id) VALUES
 (N'Sítio Boa Vista', 12.50, 'Zona rural - norte', 1),
 (N'Sítio Água Clara', 8.00,  'Zona rural - sul',   2);

INSERT INTO plantio (cultura, area_plantada, data_plantio, data_prev_colheita, propriedade_id) VALUES
 ('Tomate', 5.00, '2026-03-01', '2026-06-15', 1),
 ('Tomate', 4.00, '2026-03-10', '2026-06-20', 2);

INSERT INTO previsao_safra (plantio_id, volume_previsto, data_calculo) VALUES
 (1, 250000.00, '2026-03-05'),
 (2, 200000.00, '2026-03-12');

INSERT INTO safra (plantio_id, volume_colhido, data_colheita) VALUES
 (1, 240000.00, '2026-06-16'),
 (2, 180000.00, '2026-06-22');

INSERT INTO leitura_sensor (propriedade_id, umidade, temperatura, data_hora) VALUES
 (1, 41.5, 27.8, '2026-04-02 08:00'),
 (1, 38.0, 29.1, '2026-04-02 09:00'),
 (2, 55.2, 25.3, '2026-04-02 08:00');

INSERT INTO comprador (nome, tipo) VALUES ('Rede de Mercados Exemplo', 'varejo');

INSERT INTO negociacao (volume, preco, data, comprador_id, cooperativa_id) VALUES
 (420000.00, 2.50, '2026-06-25', 1, 1);
