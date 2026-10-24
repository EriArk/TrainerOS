#pragma once
#include "features/settings/SoftwareUpdateController.h"
#include <QFile>
#include <QDir>

// Process protocol fixture only. Signature/HTTPS/archive proof is covered by
// SoftwareChannelTests + SoftwareReleaseTests, never by this fake release.
inline trainer::SoftwareUpdateSource softwareUpdateFixture(const QString& root,const QString& python) {
    QDir().mkpath(root);
    QFile helper(root+"/worker.py");if(!helper.open(QIODevice::WriteOnly))qFatal("Cannot write update fixture");
    helper.write(R"PY(import json,sys,time
from pathlib import Path
root=Path(__file__).parent
command=sys.argv[1]
with (root/'requests').open('a') as f: f.write(command+'\n')
mode=(root/'mode').read_text() if (root/'mode').exists() else 'normal'
channel=sys.argv[sys.argv.index('--channel')+1]
def emit(value): print(json.dumps(value),flush=True)
result={'channel':channel,'version':'fixture-7','sequence':7,'notes':('Local UI fixture. No release is installed.\n'*40),'downloadBytes':30*1048576,'installed':False}
if mode=='invalid': print('not json',flush=True);sys.exit(0)
if mode=='empty': sys.exit(0)
if mode=='error': emit({'error':'fixture offline'});sys.exit(1)
if mode=='space': emit({'error':'Not enough space to prepare the complete release'});sys.exit(1)
if mode=='blocked': emit(dict(result,status='blocked',reason='data-incompatible'));sys.exit(2)
if command=='check': emit(dict(result,status='available'));sys.exit(0)
for i in range(30):
    emit({'phase':'download','bytes':i*1048576,'totalBytes':30*1048576});time.sleep(.1)
emit({'phase':'verify-archive','bytes':30*1048576,'totalBytes':30*1048576})
emit({'phase':'extract','bytes':30*1048576,'totalBytes':30*1048576})
emit({'phase':'prepared','bytes':0,'totalBytes':0})
if mode=='crash-after-phase': sys.exit(1)
if mode=='lie-installed': result['installed']=True
emit(dict(result,status='prepared',prepared=str(root/'candidate')))
)PY");helper.close();
    QFile key(root+"/key.pem");if(!key.open(QIODevice::WriteOnly))qFatal("Cannot write update fixture key");key.write("not a production key");key.close();
    trainer::SoftwareUpdateSource source;
    source.python=python;source.helper=helper.fileName();source.publicKey=key.fileName();
    source.betaUrl="https://fixture.invalid/beta.json";source.previewUrl="https://fixture.invalid/preview.json";
    source.state=root+"/state";source.cache=root+"/cache";source.installedVersion="fixture-6";source.installedSequence=6;
    source.target="linux-aarch64";source.sqliteSchema=16;source.firstRunSchema=2;
    return source;
}
inline void softwareUpdateMode(const QString& root,const QByteArray& mode) {
    QFile f(root+"/mode");if(!f.open(QIODevice::WriteOnly|QIODevice::Truncate))qFatal("Cannot set update fixture mode");f.write(mode);
}
