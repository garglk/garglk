# Flatpak

Should be suitable for submission to Flathub, if you explain why it needs read access to home directory and write access to save games.
https://docs.flathub.org/docs/for-app-authors/linter#finish-args-home-filesystem-access

## Build and install

    rm -rf .flatpak-builder repo builddir build-dir
    flatpak-builder --install --install-deps-from=flathub --force-clean --user  builddir io.github.garglk.Gargoyle.json  

## Run

    flatpak run io.github.garglk.Gargoyle 

## Validation checks

    flatpak run --command=flatpak-builder-lint org.flatpak.Builder manifest io.github.garglk.Gargoyle.json  
    flatpak run --command=flatpak-builder-lint org.flatpak.Builder appstream ../garglk/io.github.garglk.Gargoyle.appdata.xml  
    flatpak run --command=flatpak-builder-lint org.flatpak.Builder builddir builddir
    flatpak run --command=flatpak-builder-lint org.flatpak.Builder repo repo

## Standalone bundle

    flatpak build-bundle repo gargoyle.flatpak io.github.garglk.Gargoyle
