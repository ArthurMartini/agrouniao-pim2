USE agrouniao;
GO

-- Consulta 1 — produção colhida por propriedade e cultura
SELECT p.nome AS propriedade, pl.cultura, s.volume_colhido, s.data_colheita
FROM safra s
JOIN plantio pl     ON pl.id = s.plantio_id
JOIN propriedade p  ON p.id  = pl.propriedade_id
ORDER BY p.nome;

-- Consulta 2 — consolidação para negociação coletiva (RF05)
SELECT pl.cultura, SUM(s.volume_colhido) AS volume_total
FROM safra s
JOIN plantio pl ON pl.id = s.plantio_id
GROUP BY pl.cultura;

-- Consulta 3 — pagamento proporcional por cooperado (RF08 / RN02)
WITH producao AS (
    SELECT pr.id AS produtor_id, pr.nome, SUM(s.volume_colhido) AS volume
    FROM safra s
    JOIN plantio pl    ON pl.id = s.plantio_id
    JOIN propriedade p ON p.id  = pl.propriedade_id
    JOIN produtor pr   ON pr.id = p.produtor_id
    GROUP BY pr.id, pr.nome
)
SELECT nome,
       volume,
       ROUND(volume * (SELECT preco FROM negociacao WHERE id = 1), 2) AS valor_a_pagar
FROM producao
ORDER BY nome;

-- Consulta 4 — leituras recentes de uma propriedade (RF07)
SELECT TOP 10 data_hora, umidade, temperatura
FROM leitura_sensor
WHERE propriedade_id = 1
ORDER BY data_hora DESC;

-- Consulta 5 — previsto x realizado por propriedade (RF03 e RF04)
SELECT p.nome AS propriedade,
       ps.volume_previsto,
       s.volume_colhido,
       ROUND(100.0 * s.volume_colhido / ps.volume_previsto, 1) AS pct_realizado
FROM previsao_safra ps
JOIN plantio pl    ON pl.id = ps.plantio_id
JOIN propriedade p ON p.id  = pl.propriedade_id
JOIN safra s       ON s.plantio_id = pl.id;
