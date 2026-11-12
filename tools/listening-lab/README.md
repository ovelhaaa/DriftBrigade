# DriftBrigade Listening Lab

1. Extraia o ZIP **DriftBrigade-M3.0-BBD-Listening-Bakeoff** ou **DriftBrigade-M3.1B1-Organic-Voice-Retest**.
2. Abra `index.html` no Chrome ou Edge no Windows. O laboratório roda offline, sem servidor ou instalação.
3. Clique em **Selecionar pasta** e escolha a pasta extraída que contém `LISTENING_SCORECARD.csv`, `dry/`, `raw/` e `level_matched/`.
4. Comece em **LEVEL MATCHED**. Ouça o original, compare X/Y/Z e registre sua preferência. Use **RAW** para conferir as diferenças de nível dos renders.
5. Em **Resultados da audição**, exporte o scorecard CSV ou o relatório Markdown. Para conhecer as correspondências, clique em **Revelar resultados** e confirme. Isso requer o `ANSWER_KEY.csv` do pacote.

As edições são salvas no navegador por pacote. Para retomar, abra o mesmo HTML no mesmo perfil e selecione novamente a pasta. Se mudar de navegador ou mover o HTML, leve o CSV exportado e use **Importar avaliação**. A exportação baixa uma cópia do scorecard.

| Atalho | Ação |
| --- | --- |
| Espaço | Reproduzir ou pausar |
| 1 / 2 / 3 | Selecionar X / Y / Z |
| 0 | Ouvir o original |
| R | Voltar ao início |
| L | Ativar ou desativar loop |
| M | Alternar LEVEL MATCHED e RAW |

O WebAudio carrega apenas o original e as duas versões dos candidatos do grupo ativo. As fontes compartilham o relógio de reprodução e a troca aplica uma rampa de 8 ms no ganho de monitoração. A forma de onda mostra sempre o original, com o cursor no tempo da comparação.

## Testes

Use Node.js 20 ou superior. Para validar os pacotes reais, defina as variáveis para as pastas extraídas antes de executar o teste.

```powershell
$env:M3_ARTIFACT = 'C:\caminho\DriftBrigade-M3.0-BBD-Listening-Bakeoff'
$env:M31_VOICE_ARTIFACT = 'C:\caminho\DriftBrigade-M3.1B1-Organic-Voice-Retest'
node --test tools/listening-lab/package.test.js
```
