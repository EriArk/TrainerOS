.pragma library

// Hints for the built-in experience presenters; global modal/input hints stay host-owned.
function actions(shell, h) {
    if (shell.page === 0) return null
    const persona = shell.experienceModel.persona
    if (persona.picker.open) return [h("X","Search"),h("Y","Clear"),h("←→","Jump 8"),h("A","Choose"),h("B","Cancel")]
    if (persona.editing) return [h("A","Select"),h("B","Cancel")]

                if (shell.experienceView === "pokemon-guide") {
                    const dex = shell.experienceModel.pokedex
                    if (dex.zone === "art") return [h("←→","Browse"),h("A","Use image"),h("B","Cancel")]
                    if (dex.zone === "picker") return [h("A","Apply"),h("B","Cancel")]
                    let result = [h("X","Search"),h("Select","Filters")]
                    if (dex.zone === "list") result.push(h("←→","Jump 8"))
                    result.push(h("A",dex.zone === "list" ? (dex.detail.favorite ? "Unfavorite" : "Favorite") : "Select"),h(dex.zone === "rail" ? "B" : "↑",dex.zone === "rail" ? "Entries" : "Filters"))
                    return result
                }
                if (shell.page === 2 && shell.experienceModel.centerFace) {
                    const party = shell.experienceModel.party
                    if (shell.experienceModel.center.shopsOpen) {
                        const route = shell.experienceModel.center.shopRoute
                        if (shell.experienceModel.center.busy) return []
                        if (route === "merchants") return [h("←→","Categories"),h("X","Search"),h("Y","Place"),h("Select","Basket · "+shell.experienceModel.center.basketCount),h("A","Open"),h("B","Back")]
                        if (route === "basket") return shell.experienceModel.center.basketCount ? [h("←→","Quantity"),h("X","Remove"),h("A","Buy basket"),h("B","Back")] : [h("A","Back"),h("B","Back")]
                        if (route === "locations") return [h("A","Choose"),h("B","Back")]
                        if (route === "stock") return shell.experienceModel.center.shopSelection.lesson ? [h("A","Choose Pokémon"),h("B","Tutors")] : [h("←→","Quantity"),h("A","Add"),h("Select","Basket · "+shell.experienceModel.center.basketCount),h("B","Shops")]
                        return [h("A",route === "confirm" ? "Confirm" : "Select"),h("B","Back")]
                    }
                    if (shell.experienceModel.center.clinicOpen) return shell.experienceModel.center.busy ? [] : [h("Select","Backups"),h("X","Link"),h("A",shell.experienceModel.center.treatment === "ready" && shell.experienceModel.center.canHeal ? "Heal team" : "OK")]
                    if (party.section === "saves") return shell.experienceModel.center.confirming ? [h("A","Restore"),h("B","Cancel")] : [h("X",shell.experienceModel.center.route === "adventures" ? "Search" : "Refresh"),h("Select","Backup"),h("A","Open"),h("B","Back")]
                    if (party.section === "activities" && party.activities.route === "practice") {
                        const practice = party.activities.practice
                        if (practice.stage === "starting" || practice.stage === "waiting") return [h("B","Leave practice")]
                        if (practice.stage === "first" && !practice.ready) return [h("A","Back"),h("B","Back")]
                        return [h("A",practice.stage === "ready" ? "Begin" : practice.stage === "events" ? "Next" : practice.stage === "finished" ? "Again" : practice.stage === "moves" ? "Move" : "Choose"),h("B",practice.running ? "Leave practice" : "Back")]
                    }
                    if (party.section === "activities" && party.activities.route === "link") {
                        const link = party.activities.link
                        if (link.stage === "browse") return link.rows.length ? [h("A","Connect"),h("B","Back")] : [h("B","Back")]
                        if (link.stage === "lobby") return [h("X","Disconnect"),h("A","Invite"),h("B","Back")]
                        if (link.stage === "events") return []
                        if (link.stage === "price") return [h("↑↓","Price ±" + link.priceStep),h("←→","Step"),h("A","Offer"),h("B","Cancel")]
                        if (link.stage === "concede") return [h("A","Concede"),h("B","Keep battling")]
                        if (link.stage === "moves") return [h("X","Team"),h("Y","Bag"),h("A",link.battlePanel === "target" ? "Use" : "Choose"),h("B",link.battlePanel !== "moves" ? "Attacks" : "Concede")]
                        if (link.stage === "stake" || link.stage === "choose" && link.mode !== "battle") return [h("X Y","Party / Boxes"),h("A","Choose"),h("B","Back")]
                        if (link.canSetTerms) return (link.stakeText.indexOf("₽") >= 0 ? [h("↑↓","Amount ±" + link.priceStep),h("←→","Step")] : []).concat([h("Select","Stake"),h("A","Ready"),h("B","Back")])
                        return [h("A",link.stage === "pair" ? "Connect" : link.stage === "review" ? "Confirm" : link.stage === "moves" ? "Move" : "Choose"),h("B",link.pending ? "Pause" : "Back")]
                    }
                    if (party.section === "activities") return party.activities.route === "playroom" && party.activities.hasParty ? [h("↑","Practice"),h("←→","Partner"),h("Select","Play"),h("X","Greet"),h("A","Call")] : [h("A","Select"),h("B","Back")]
                    if (party.moveStage === "release-confirm") return [h("X","Release"),h("B","Keep Pokemon")]
                    if (party.moveOpen) return party.moveStage === "writing" || party.moveStage === "checking" ? [] : [h("A",party.moveStage === "name-confirm" ? "Rename" : party.moveStage === "name-error" ? "Edit name" : party.moveStage === "item-confirm" ? "Confirm" : party.moveStage === "confirm" ? "Confirm" : party.moveStage === "result" ? "OK" : "Choose"),h("B",party.moveStage === "places" ? "Cancel" : "Back")]
                    if (party.detailOpen) return [h("A","Select"),h("B","Close")]
                    if (party.boxFocused) return (party.canRenameBox ? [h("X","Rename box")] : []).concat([h("←→","Box"),h("↓","Slots"),h("B","Back")])
                    const actions = [h("Select","Backups"),h("A",party.available && !party.activitiesFocused ? "Actions" : "Open"),h("B","Back")]
                    if (party.canRenameBox) actions.unshift(h("X","Rename box"))
                    if (party.section === "storage" && party.available && !party.activitiesFocused && party.focusIndex < 6) actions.unshift(h("↑","Boxes"))
                    return actions
                }
                if (shell.experienceModel.historyFace) {
                    const hall = shell.hall
                    if (hall.editor.open) return hall.editor.route === "form" ? [h("Y","Save"),h("A","Edit"),h("B","Discard")] : [h("X",hall.editor.route === "team" ? "Level" : "Search"),h("A",hall.editor.route === "team" ? "Name" : "Choose"),h("B","Back")]
                    let result = []
                    if (hall.archive && hall.editable) result.push(h("Select","New memory"))
                    if (!hall.archive) result.push(h("Select","Refresh"))
                    if (!hall.archive && hall.account.available) result.push(h("X","Account"))
                    else if (hall.route === "archive-journey") result.push(h("X","Champions"))
                    else if (hall.archive && hall.editable && hall.rows.length && !hall.overview) result.push(h("X","Edit"))
                    if (hall.route === "archive-champions" || hall.route === "archive-champion-detail") result.push(h("← →","Records"))
                    result.push(h("A",hall.overview ? (hall.route === "archive-journey" ? "Champions" : "Journey") : "Open"),h("B","Back")); return result
                }
    return null
}
