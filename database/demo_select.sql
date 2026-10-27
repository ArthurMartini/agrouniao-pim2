-- Demonstracao C <-> PostgreSQL: rodar logo apos registrar a operacao no programa
-- psql -d agrouniao -f database/demo_select.sql
SELECT pl.id, pr.nome AS produtor, pl.cultura, pl.area_plantada,
       ps.volume_previsto, s.volume_colhido, s.data_colheita
FROM plantio pl
JOIN propriedade pp ON pp.id = pl.propriedade_id
JOIN produtor   pr ON pr.id = pp.produtor_id
LEFT JOIN previsao_safra ps ON ps.plantio_id = pl.id
LEFT JOIN safra s ON s.plantio_id = pl.id
ORDER BY pl.id DESC
LIMIT 5;
