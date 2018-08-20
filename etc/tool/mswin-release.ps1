Add-Type -A 'System.IO.Compression.FileSystem';

$OUTDIR='out/mswin-default';
$STAGING=$OUTDIR+'/pkg';
$ARCHIVE=$OUTDIR+'/plundersquad-mswin-'+$(Get-Date -Format 'yyyyMMdd-HHmm')+'.zip';

if (Test-Path $STAGING) {
  rm -Recurse $STAGING;
}
mkdir $STAGING | Out-Null;

Copy-Item $OUTDIR'/plundersquad.exe' $STAGING
Copy-Item $OUTDIR'/plundersquad.cfg' $STAGING
Copy-Item $OUTDIR'/input.cfg' $STAGING
Copy-Item $OUTDIR'/ps-data' $STAGING

rm $OUTDIR/plundersquad-*.zip
[IO.Compression.ZipFile]::CreateFromDirectory($STAGING,$ARCHIVE);

rm -Recurse $STAGING;
