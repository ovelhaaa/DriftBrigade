"use strict";

const categories = {
  A: ["Arquitetura do delay", "A mesma modulação move um delay digital fracionário e um BBD completo. Compare como a textura de cada caminho se mistura à fonte.", "backend"],
  B: ["Número de estágios", "O BBD alterna entre 512 e 1024 estágios, com os respectivos limites de clock. A resposta tonal própria de cada opção foi preservada.", "stages"],
  C: ["Ganho operacional", "Dois perfis de ganho interno, Conservative e Nominal, mudam o nível com que o sinal percorre o BBD. Ouça como ele reage a sinais fracos e a picos.", "gain_profile"],
  D: ["Movimento orgânico", "Três trajetórias de modulação, cada uma com sua forma de movimento.", "organic_variant"],
  E: ["Banco multibanda", "Gentle e Selective dividem o espectro de maneiras diferentes. Ouça se as faixas parecem se mover juntas ou em camadas.", "bank_mode"],
};

const materials = {
  A: "Acordes sustentados de pad, com ataques suaves e sobreposição de notas.",
  B: "Frase dedilhada com ataques curtos e percussivos.",
  C: "Lead harmônico parecido com uma voz, duplicado nos canais L/R a partir de uma fonte mono.",
};

// The first three entries mirror the public scorecard metadata.
const guide = {
  A01: ["A", "subtle", "XY", "Chorus discreto no pad", "No início do acorde, por volta de 0,5 s, ouça ruído e suavidade. Durante a sustentação, a cor do efeito se integra ao pad?", "Um chorus que se mistura ao arranjo ou uma textura mais evidente."],
  A02: ["A", "wide", "XY", "Chorus amplo no pad", "Nos acordes sustentados, acompanhe o movimento entre esquerda e direita. A textura parece lisa, granulada ou metálica?", "Uma imagem ampla e estável ou um movimento que chama atenção."],
  A03: ["B", "feedback", "XY", "Ataques e ecos", "Siga os ataques repetidos a partir de 0,5 s. O efeito fica ligado à nota? Depois de 7,5 s, ouça a definição da cauda.", "Ataques definidos ou um rastro que preenche os espaços da frase."],
  A04: ["C", "slow", "XY", "Deriva lenta na voz", "Nas notas longas e nas transições, acompanhe a afinação e a largura criada a partir do centro. A mudança parece contínua?", "Uma voz firme no centro ou uma voz que se abre e flutua."],
  B01: ["A", "wide", "XY", "Cor do pad em estéreo", "Durante a sustentação e a troca de acordes, ouça brilho, densidade e textura de modulação.", "Um pad mais aberto ou uma cor mais escura e densa."],
  B02: ["B", "short", "XY", "Dedilhado com delay curto", "Nos ataques a partir de 0,5 s, ouça mudanças na definição e uma possível cor oca ou metálica.", "Ataques próximos do original ou a coloração de um delay curto."],
  C01: ["A", "subtle", "XY", "Pad em nível baixo", "Ouça o início em torno de 0,5 s, o corpo das notas e o fim da cauda. O sinal de entrada dos candidatos tem 1/8 da amplitude da referência original.", "Um fundo discreto ou uma textura audível mesmo nas partes suaves."],
  C02: ["B", "feedback", "XY", "Ganho nos ataques e na cauda", "Ouça se os primeiros ataques ficam ásperos ou arredondados. Depois de 7,5 s, observe como o feedback se apaga.", "Ataques incisivos ou arredondados, com a cauda que melhor serve à frase."],
  D01: ["A", "wide", "XYZ", "Movimento nos acordes", "No meio dos acordes, siga as oscilações de afinação. O movimento se repete, vagueia ou muda de direção de forma contínua?", "Um pulso previsível ou um movimento menos regular."],
  D02: ["C", "wide_high_chaos", "XYZ", "Voz com movimento intenso", "Nas notas sustentadas, ouça se a afinação parece viva, instável ou repetitiva. A sensação persiste ao longo da frase?", "Um lead estável ou uma oscilação que faça parte da expressão."],
  E01: ["A", "wide", "XY", "Camadas de um pad amplo", "Nos acordes sobrepostos, siga graves e agudos. As regiões parecem se mover como um conjunto ou se destacar separadamente?", "Uma textura coesa ou camadas que ocupam espaços diferentes."],
  E02: ["B", "subtle", "XY", "Camadas nos ataques", "Nos primeiros ataques e nos intervalos entre notas, ouça se surge uma borda metálica e se o efeito se integra ao ataque.", "Um dedilhado coeso ou uma separação que realça detalhes."],
};

const steps = [
  ["Conheça o material", "Ouça o original para reconhecer a fonte. Os candidatos trazem essa mesma frase processada. Ao alternar X, Y ou Z, você compara implementações no mesmo ponto."],
  ["Compare os candidatos", "Comece em LEVEL MATCHED. Alterne os rótulos enquanto toca e mantenha o volume do monitor fixo."],
  ["Escute os detalhes", ""],
  ["Registre a preferência", "Escolha o que serviria melhor à sua música. Use as notas para lembrar trechos e diferenças entre os dois modos."],
  ["Confira o nível original", "Repita em RAW para ouvir as diferenças de nível próprias do processamento."],
  ["Avance para o próximo teste", "As avaliações são salvas ao editar. Você pode voltar a qualquer teste."],
];

const voiceGuide = {
  D03: ["C", "voice_moderate_chaos", "XYZ", "Voz com Chaos 50%", "Qual versão soa mais natural e tem movimento mais interessante? Alguma parece repetitiva ou desagradavelmente instável?", "Escolha a versão que melhor preserva o caráter do lead."],
  D04: ["C", "voice_full_chaos", "XYZ", "Voz com Chaos 100%", "O movimento fica mais expressivo? Alguma versão exagera na instabilidade? Qual mantém a continuidade da afinação?", "Escolha a versão que usaria numa gravação. Compare com D03 após revelar as identidades."],
};
const packageProfiles = [
  { version: "M3.0", guide },
  { version: "M3.1B.1", guide: voiceGuide },
];

if (typeof module !== "undefined") module.exports = { guide, voiceGuide, categories, packageProfiles };
