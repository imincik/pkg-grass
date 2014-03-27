#!/bin/sh

# this script tries to locate all the GRASS scripts than have something
# that makes lintian complain and fix them.

CURDIR=$(pwd)
VERSION=$(echo $(head -2 $CURDIR/include/VERSION)|sed -e 's/ //') 

# silence bogus lintian complaint about interpreter-not-absolute
for SCRIPT in script_get_line \
    script_play \
    script_tools \
    script_file_tools \
    nviz2.2_script
do
  file="$CURDIR/debian/tmp/usr/lib/grass$VERSION/etc/nviz2.2/scripts/$SCRIPT"
  sed -i -e "s.!nviz.!/usr/lib/grass$VERSION/bin/nviz." "$file"
done

for SCRIPT in panel_label.tcl \
    panel_scale.tcl
do
  file="$CURDIR/debian/tmp/usr/lib/grass$VERSION/etc/nviz2.2/scripts/$SCRIPT"
  sed -i -e "s%!../glnviz.new/nvwish%!/usr/lib/grass$VERSION/etc/nviz2.2/glnviz/nvwish%" "$file"
done

# silence lintian warning script-not-executable
for x in etc/dm/tksys.tcl \
    etc/gm/animate.tcl \
    etc/gem/skeleton/post \
    etc/gem/skeleton/uninstall
do
    chmod +x $CURDIR/debian/tmp/usr/lib/grass$VERSION/$x
done

# silence executable-not-elf-or-script lintian warning
# most tcl scripts don't need to be executable
for x in etc/dm/cmd.tcl \
    etc/dm/d.m.tcl \
    etc/dm/grassabout.tcl \
    etc/dm/group.tcl \
    etc/dm/labels.tcl \
    etc/dm/menu.tcl \
    etc/dm/print.tcl \
    etc/dm/raster.tcl \
    etc/dm/tool1.tcl \
    etc/dm/tool2.tcl \
    etc/dm/tree.tcl \
    etc/dm/vector.tcl \
    etc/epsg_option.tcl \
    etc/gis_set.tcl \
    etc/gm/tksys.tcl \
    etc/nviz2.2/scripts/assoc.tcl \
    etc/nviz2.2/scripts/attIsosurfPopup.tcl \
    etc/nviz2.2/scripts/attPopup.tcl \
    etc/nviz2.2/scripts/colorPopup.tcl \
    etc/nviz2.2/scripts/config.tcl \
    etc/nviz2.2/scripts/cutplane_channels.tcl \
    etc/nviz2.2/scripts/extra_bindings.tcl \
    etc/nviz2.2/scripts/fileBrowser.tcl \
    etc/nviz2.2/scripts/filemapBrowser.tcl \
    etc/nviz2.2/scripts/mapBrowser.tcl \
    etc/nviz2.2/scripts/multimapBrowser.tcl \
    etc/nviz2.2/scripts/nviz_init.tcl \
    etc/nviz2.2/scripts/panelIndex \
    etc/nviz2.2/scripts/panel_animation.tcl \
    etc/nviz2.2/scripts/panel_color.tcl \
    etc/nviz2.2/scripts/panel_cutplane.tcl \
    etc/nviz2.2/scripts/panel_kanimator.tcl \
    etc/nviz2.2/scripts/panel_label.tcl \
    etc/nviz2.2/scripts/panel_lights.tcl \
    etc/nviz2.2/scripts/panel_main.tcl \
    etc/nviz2.2/scripts/panel_pos.tcl \
    etc/nviz2.2/scripts/panel_rquery.tcl \
    etc/nviz2.2/scripts/panel_scale.tcl \
    etc/nviz2.2/scripts/panel_sdiff.tcl \
    etc/nviz2.2/scripts/panel_site.tcl \
    etc/nviz2.2/scripts/panel_surf.tcl \
    etc/nviz2.2/scripts/panel_tst.tcl \
    etc/nviz2.2/scripts/panel_vect.tcl \
    etc/nviz2.2/scripts/panel_vquery.tcl \
    etc/nviz2.2/scripts/panel_vol.tcl \
    etc/nviz2.2/scripts/position_procs.tcl \
    etc/nviz2.2/scripts/queue.tcl \
    etc/nviz2.2/scripts/script_support.tcl \
    etc/nviz2.2/scripts/send_support.tcl \
    etc/nviz2.2/scripts/structlib.tcl \
    etc/nviz2.2/scripts/tclIndex \
    etc/nviz2.2/scripts/unique.tcl \
    etc/nviz2.2/scripts/widgets.tcl \
    etc/nviz2.2/scripts/wirecolorPopup.tcl \
    etc/v.digit/cats.tcl \
    etc/v.digit/settings.tcl \
    etc/v.digit/toolbox.tcl
do
    chmod -x $CURDIR/debian/tmp/usr/lib/grass$VERSION/$x
done

