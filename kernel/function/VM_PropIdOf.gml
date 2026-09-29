function VM_PropIdOf(_name) {
    switch (_name) {
        case "x":            return 1;
        case "y":            return 2;
        case "sprite_index": return 3;
        case "image_xscale": return 4;
        case "image_yscale": return 5;
        case "depth":        return 6;
        case "image_angle":  return 7;
        case "image_alpha":  return 8;
        case "image_index":  return 9;
    }
    return 0;
}
