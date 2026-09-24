.pragma library

// Original schematic scenery. Identity follows World ID, including renamed Worlds.
var themes = {
    kanto: ["#bfe881", "#4a7154", "fields"],
    johto: ["#d3e88d", "#626c44", "shrine"],
    hoenn: ["#88dedb", "#287b80", "waves"],
    sinnoh: ["#adcfee", "#526789", "mountains"],
    unova: ["#b4c5ed", "#536579", "city"],
    kalos: ["#e7bcec", "#82638b", "garden"],
    alola: ["#a3e4ba", "#398368", "leaves"],
    galar: ["#dfb8df", "#7b6550", "stadium"],
    paldea: ["#f5d47c", "#9a743d", "routes"],
    hisui: ["#c6d5d1", "#4d7973", "mountains"],
    orre: ["#dfbd92", "#98704d", "dunes"],
    fiore: ["#b7d4ae", "#547d52", "forest"],
    almia: ["#aacebd", "#3b7962", "forest"],
    oblivia: ["#b2d4db", "#447f8e", "waves"],
    ransei: ["#dcc8a6", "#8a7047", "shrine"],
    lental: ["#b9d9c5", "#4c8472", "leaves"],
    "pokemon-island": ["#a8d3cf", "#397b7b", "waves"],
    ferrum: ["#d6b7ad", "#926456", "stadium"],
    poketopia: ["#c3bedc", "#746a99", "stadium"],
    pokepark: ["#c7dcac", "#65854c", "forest"],
    "ryme-city": ["#bbcad5", "#576d83", "city"],
    "mystery-dungeon": ["#cfbead", "#877051", "mountains"],
    "toy-world": ["#dcc3b6", "#986c57", "city"],
    "tcg-islands": ["#b5d3da", "#58808c", "waves"],
    "tumblecube": ["#ddc99e", "#8b754b", "routes"],
    "think-town": ["#c8d5b5", "#688253", "garden"],
    puzzles: ["#d3c4dd", "#836891", "garden"],
    playroom: ["#e5c9b0", "#9b7b55", "fields"],
    crossovers: ["#c0d0e0", "#617c97", "stadium"],
    pasio: ["#afd8d1", "#427e78", "waves"],
    aeos: ["#c8c8e0", "#716d99", "stadium"],
    "our-world": ["#bfd7c7", "#597e6a", "routes"],
    "card-club": ["#dbc5b4", "#8f705a", "garden"]
}
function theme(id) { return themes[id] || ["#b9ddb1", "#487456", "routes"] }
// Material and scenery are separate: colored elements never become grey silhouettes.
function scenery(motif) {
    var colors = {
        fields: ["#91c775", "#f7cf79", "#d9aeec", "#8cceb2"],
        shrine: ["#85c69b", "#a6cf85", "#e79b8b", "#8ac3b6"],
        forest: ["#6dbb9b", "#9dcf78", "#b3a3df", "#e7b87f"],
        waves: ["#75b7e9", "#88d8cf", "#a8c984", "#d2a8dd"],
        mountains: ["#a495cf", "#8cbfcf", "#d4a9ca", "#d5e9f1"],
        city: ["#929dd7", "#dfa49a", "#87c9c5", "#e6c27c"],
        garden: ["#c28ede", "#efa2ba", "#8bc6ad", "#e8c57b"],
        leaves: ["#66c09a", "#a5cf6d", "#82c8c1", "#e7ac91"],
        stadium: ["#a198d1", "#e0a088", "#84bccb", "#dfc578"],
        dunes: ["#e4ac78", "#e39fa4", "#bd9ed6", "#ecc979"],
        routes: ["#a3c97d", "#efb27d", "#af9cdd", "#8ebfcf"]
    }
    return colors[motif] || colors.routes
}
