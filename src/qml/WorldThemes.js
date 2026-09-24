.pragma library

// Original schematic scenery. Identity follows World ID, including renamed Worlds.
var themes = {
    kanto: ["#c5d99a", "#4a7154", "fields"],
    johto: ["#cbd3a0", "#626c44", "shrine"],
    hoenn: ["#a8d8d5", "#287b80", "waves"],
    sinnoh: ["#bdcfe1", "#526789", "mountains"],
    unova: ["#bfccd9", "#536579", "city"],
    kalos: ["#d7c8df", "#82638b", "garden"],
    alola: ["#b8dcc0", "#398368", "leaves"],
    galar: ["#d5c9b2", "#7b6550", "stadium"],
    paldea: ["#ecd197", "#9a743d", "routes"],
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
function theme(id) { return themes[id] || ["#c5d3bd", "#627a60", "routes"] }
