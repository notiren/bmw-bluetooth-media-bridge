// BMW MEDIA enclosure: ESP32 DevKit (30-pin) + PCM5102A + SN65HVD230.
// Boards slide into rails (lip at the front, detent at the back); snap-fit lid.
// Sizes are typical values: measure your boards and adjust them below.
//
// Export:  openscad -D "part=\"box\"" -o box.stl enclosure.scad
//          openscad -D "part=\"lid\"" -o lid.stl enclosure.scad

part = "assembly"; // [assembly, box, lid]

/* [Enclosure] */
wall = 2;
floor_t = 2;
lid_t = 2;
inner = [78, 57, 21];
corner_r = 3;
fit = 0.25;      // lid skirt clearance per side
skirt_h = 4;
skirt_t = 1.2;
snap = 0.5;      // snap ridge height (more = tighter)

/* [Rails] */
pcb_t = 1.6;
rail_cl = 0.3;   // side play
fin_t = 1.2;
lip = 0.7;       // lip over the PCB edge (stay clear of solder joints)
lip_t = 1.0;

/* [ESP32 DevKit, USB through the left wall] */
esp = [51.5, 28.5];
esp_h = 10;      // PCB bottom above the floor (room for header pins)
esp_rib = 14;    // support rib between the two pin rows
esp_slide = 6;
esp_y = inner.y - 1.6 - esp.y;
usb = [9.5, 4.5];            // receptacle opening (w, h)
usb_zc = 1.3;                // receptacle centre above the PCB top
usb_bore = [12.5, 8.5, 1];   // outside recess for the plug overmold
esp_buttons = [[3, 4.5], [3, esp.y - 4.5]];  // EN / BOOT (from USB end, board edge)
esp_led = [11, esp.y / 2 + 7];

/* [PCM5102A, jack through the front wall] */
pcm = [32, 20];
pcm_x = 4;
pcm_h = 4;
pcm_rib = 10;
pcm_slide = 3.5;
jack_x = 8;      // jack centre from the board's left edge
jack_zc = 2.6;   // jack centre above the PCB top
jack_d = 7;
jack_bore = [10, 1];

/* [SN65HVD230, K-CAN through the right wall] */
sn = [22, 15];
sn_y = 3;
sn_gap = 4;      // board end to the right wall (room for wires)
sn_h = 4;
sn_rib = 8;
sn_slide = 4;
can_d = 4.5;
can_zc = 6;

/* [Lid] */
label = "BMW MEDIA";
label_size = 7;
label_depth = 0.6;
btn_d = 2;
led_d = 3;

$fn = 48;
outer = [inner.x + 2 * wall, inner.y + 2 * wall, inner.z + floor_t];
snap_z = outer.z - skirt_h / 2;
sn_x = inner.x - sn_gap - sn.x;

module rbox(s, r) {
    hull() for (x = [r, s.x - r], y = [r, s.y - r])
        translate([x, y, 0]) cylinder(r = r, h = s.z);
}

// Board rails in local coordinates: board at x 0..L, y 0..W, PCB bottom at z = h.
// The board slides toward -x; lips hold its front edge, a bump stops it at the back.
module rails(b, h, rib, slide, stop = false) {
    L = b.x; W = b.y;
    top = h + pcb_t + 0.2;
    lip_len = slide - 0.5;
    translate([0, (W - rib) / 2, 0]) cube([L + 2.5, rib, h]);
    translate([L + 0.2, (W - rib) / 2, h]) rotate([90, 0, 0]) translate([0, 0, -rib])
        linear_extrude(rib) polygon([[0, 0], [1, 0.6], [2, 0]]);
    for (s = [0, 1]) {
        translate([0, s == 0 ? -rail_cl - fin_t : W + rail_cl, 0])
            cube([L, fin_t, top + lip_t]);
        translate([0, s == 0 ? -rail_cl - fin_t : W - lip, top])
            cube([lip_len, fin_t + rail_cl + lip, lip_t]);
    }
    if (stop) translate([-1.5, 0, 0]) cube([1.5, W, h + pcb_t]);
}

module box() {
    usb_yc = wall + esp_y + esp.y / 2;
    usb_zc_abs = floor_t + esp_h + pcb_t + usb_zc;
    jack_xc = wall + pcm_x + jack_x;
    jack_zc_abs = floor_t + pcm_h + pcb_t + jack_zc;
    difference() {
        rbox(outer, corner_r);
        translate([wall, wall, floor_t]) rbox([inner.x, inner.y, inner.z + 1], corner_r - wall);
        // USB
        translate([-1, usb_yc - usb.x / 2, usb_zc_abs - usb.y / 2]) cube([wall + 2, usb.x, usb.y]);
        translate([-1, usb_yc - usb_bore.x / 2, usb_zc_abs - usb_bore.y / 2])
            cube([1 + usb_bore.z, usb_bore.x, usb_bore.y]);
        // 3.5 mm jack
        translate([jack_xc, -1, jack_zc_abs]) rotate([-90, 0, 0]) {
            cylinder(d = jack_d, h = wall + 2);
            cylinder(d = jack_bore[0], h = 1 + jack_bore[1]);
        }
        // K-CAN cable
        translate([outer.x - wall - 1, wall + sn_y + sn.y / 2, floor_t + can_zc])
            rotate([0, 90, 0]) cylinder(d = can_d, h = wall + 2);
        // snap grooves
        for (y = [wall - 0.7, outer.y - wall])
            translate([outer.x / 2 - 9, y, snap_z - 0.6]) cube([18, 0.7, 1.2]);
        // pry notch for the lid
        translate([outer.x / 2 - 5, -1, outer.z - 1.2]) cube([10, wall + 0.5, 2]);
    }
    translate([wall, wall + esp_y, floor_t]) rails(esp, esp_h, esp_rib, esp_slide);
    translate([wall + pcm_x + pcm.x, wall, floor_t]) rotate([0, 0, 90])
        rails([pcm.y, pcm.x], pcm_h, pcm_rib, pcm_slide);
    translate([wall + sn_x + sn.x, wall + sn_y + sn.y, floor_t]) rotate([0, 0, 180])
        rails(sn, sn_h, sn_rib, sn_slide, stop = true);
}

module lid() {
    o = wall + fit;
    s = [inner.x - 2 * fit, inner.y - 2 * fit];
    difference() {
        union() {
            translate([0, 0, outer.z]) rbox([outer.x, outer.y, lid_t], corner_r);
            translate([o, o, outer.z - skirt_h]) difference() {
                rbox([s.x, s.y, skirt_h], corner_r - wall);
                translate([skirt_t, skirt_t, -1])
                    rbox([s.x - 2 * skirt_t, s.y - 2 * skirt_t, skirt_h + 2], corner_r - wall);
            }
            for (y = [o - snap, outer.y - o])
                translate([outer.x / 2 - 8, y, snap_z - 0.5]) cube([16, snap, 1]);
        }
        translate([outer.x / 2, wall + 13, outer.z + lid_t - label_depth])
            linear_extrude(1) text(label, size = label_size, halign = "center", valign = "center",
                                   font = "Liberation Sans:style=Bold");
        for (p = esp_buttons)
            translate([wall + p.x, wall + esp_y + p.y, outer.z - 1]) cylinder(d = btn_d, h = lid_t + 2);
        translate([wall + esp_led.x, wall + esp_y + esp_led.y, outer.z - 1]) cylinder(d = led_d, h = lid_t + 2);
        for (i = [0 : 3])
            translate([outer.x - 22 + i * 4.5, outer.y - 26, outer.z - 1]) cube([2, 18, lid_t + 2]);
    }
}

module dummy_boards() {
    ez = floor_t + esp_h + pcb_t;
    color("black") translate([wall, wall + esp_y, floor_t + esp_h]) cube([esp.x, esp.y, pcb_t]);
    color("silver") translate([wall + esp.x - 19, wall + esp_y + (esp.y - 18) / 2, ez]) cube([18, 18, 3.1]);
    color("silver") translate([wall - 1, wall + esp_y + esp.y / 2 - 4, ez]) cube([5.5, 8, 2.6]);
    for (p = esp_buttons) color("white") translate([wall + p.x - 1.5, wall + esp_y + p.y - 1.5, ez]) cube([3, 3, 1.5]);
    for (y = [1.55, esp.y - 1.55], i = [0 : 14])
        color("gold") translate([wall + 7 + i * 2.54, wall + esp_y + y, floor_t + esp_h - 8.5]) cylinder(d = 0.8, h = 8.5, $fn = 8);
    pz = floor_t + pcm_h + pcb_t;
    color("purple") translate([wall + pcm_x, wall, floor_t + pcm_h]) cube([pcm.x, pcm.y, pcb_t]);
    color("black") translate([wall + pcm_x + 15, wall + 6, pz]) cube([7, 5, 1]);
    color("silver") translate([wall + pcm_x + jack_x - 3, wall + 0.5, pz]) cube([6, 12, 5]);
    color("silver") translate([wall + pcm_x + jack_x, wall, pz + jack_zc]) rotate([-90, 0, 0]) cylinder(d = 5, h = 1);
    sz = floor_t + sn_h + pcb_t;
    color("blue") translate([wall + sn_x, wall + sn_y, floor_t + sn_h]) cube([sn.x, sn.y, pcb_t]);
    color("black") translate([wall + sn_x + 8, wall + sn_y + 5, sz]) cube([5, 4, 1.5]);
    color("green") translate([wall + sn_x + sn.x - 7, wall + sn_y + 2, sz]) cube([6.5, 11, 8]);
    color("orange") translate([wall + sn_x + sn.x, wall + sn_y + sn.y / 2 - 1, floor_t + can_zc]) rotate([0, 90, 0]) cylinder(d = 1.6, h = 12, $fn = 12);
    color("green") translate([wall + sn_x + sn.x, wall + sn_y + sn.y / 2 + 1, floor_t + can_zc]) rotate([0, 90, 0]) cylinder(d = 1.6, h = 12, $fn = 12);
}

lid_lift = 15;
show_lid = true;
lid_alpha = 0.5;

if (part == "box") box();
else if (part == "lid") translate([0, outer.y, outer.z + lid_t]) rotate([180, 0, 0]) lid();  // print top face down
else {
    color("dimgray") box();
    dummy_boards();
    if (show_lid) color("dimgray", lid_alpha) translate([0, 0, lid_lift]) lid();
}
