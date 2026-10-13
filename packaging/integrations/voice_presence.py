"""Project LiveKit's speaker events onto Fluxer accounts; no audio detector."""
import re


def account(identity):
    match = re.fullmatch(r'user_([0-9]{1,20})_(.{1,128})', str(identity))
    return match.group(1) if match else ''


def presence(participants, active, microphone_source, local_identity='', local_muted=True):
    people, speakers = set(), set()
    for participant in participants:
        person = account(participant.identity)
        if not person:
            continue
        people.add(person)
        if participant.identity not in active or (participant.identity == local_identity and local_muted):
            continue
        # A screen share or a camera alone must never be presented as a microphone.
        if any(p.source == microphone_source and not p.muted for p in participant.track_publications.values()):
            speakers.add(person)
    return {'people': sorted(people)[:100], 'speakers': sorted(speakers & people)[:100]}
