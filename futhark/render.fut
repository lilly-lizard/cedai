
let update (index: i32) (width: i32) (height: i32): [4]u8 =
    [u8.i32 (index %% width * 255 / width), u8.i32 (index // width * 255 / height), 255, 255]

let main (width: i32) (height: i32): [][4]u8 =
    let indices = iota (width * height)
    in map (\i -> update i width height) indices
