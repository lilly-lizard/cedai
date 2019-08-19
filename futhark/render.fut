
-- types
type Sphere = {pos: [3]f32, radius: f32, color: [4]u8}
type Intersection = {t: f32, index: i32, prim: u8}

-- constants
let DROP_OFF = 100f32
-- ray intersection cases
let P_NONE = 0:u8
let P_SPHERE = 1:u8
let P_LIGHT = 2:u8
let P_POLYGON = 3:u8

-- render functions:

let dot [n] (a: [n]f32) (b: [n]f32): f32 =
    f32.sum (map2 (*) a b)

let normalize [n] (a: [n]f32): [n]f32 =
    let length = f32.sqrt (f32.sum (map (\x -> x * x) a))
    in map (\x -> x / length) a

let sphereIntersect (rayO: [3]f32) (rayD: [3]f32) (s: Sphere): f32 =
    let d = map2 (-) s.pos rayO
    let b = dot d rayD
    let c = (dot d d) - s.radius * s.radius
    let disc = b * b - c
    in if (disc < 0) then DROP_OFF
    else let t = b - f32.sqrt disc
        in if (0 < t) then t else DROP_OFF

-- let clamp

-- calculates the diffusion multiplier
let sphereDiffuse (normal: [3]f32) (intersection: [3]f32) (light: [3]f32): f32 =
    f32.maximum [(dot (normalize normal) (normalize (map2 (-) light intersection))), 0:f32]

-- calculates the color of a sphere with given light sources
let sphereLighting (intersection: [3]f32) (sphere: Sphere) (lights: []Sphere): [4]u8 =
    let light = 1:f32
    -- multiply the color components by the level of light
    in (map (\c -> u8.f32 ((f32.u8 c) * light)) sphere.color[0:3])
        ++ [sphere.color[3]] -- unchanged alpha value

-- gets the background color depending on the direction you're looking
let background (rayD: [3]f32): [4]u8 =
    let color: [4]f32 = map (\c -> f32.abs c * 0.5 + 0.3) rayD ++ [1]
    in map (\c -> u8.f32 (c * 255)) color

-- render function
let render (dim: [2]i32)
           -- spheres and lights
           (numS: i32)
           (numL: i32)
           (spheres: []Sphere)
           -- return a color for each pixel
           : [][4]u8 =

    let pixelIndices = iota (dim[0] * dim[1])
    let rayO: [3]f32 = [0, 0, 0]
    -- primitive constants
    let sIndices = iota (numS + numL)
    let sTypes = (replicate numS P_SPHERE) ++ (replicate numL P_LIGHT)
    -- for each pixel...
    in map (\i ->
            let coord = [i %% dim[0], i // dim[0]]
            let rayD: [3]f32 = normalize [r32 dim[0],
                                          r32 (coord[0] - dim[0] / 2),
                                          r32 (dim[1] / 2 - coord[1])]

            -- sphere and light intersection tests for this ray
            let intersections: []Intersection = map3 (\t index prim -> {t, index, prim})
                (map (\sphere ->
                    sphereIntersect rayO rayD sphere
                ) spheres)
                sIndices sTypes

            -- closest ray intersection
            let min: Intersection = reduce (\min x->
                    if x.t < min.t then x else min
                ) {t = DROP_OFF, index = 0i32, prim = P_NONE} intersections

            -- return color
            in if (min.prim == P_SPHERE)
            then unsafe (spheres[min.index].color)
            else if (min.prim == P_LIGHT)
            then unsafe (spheres[min.index].color)
            else background rayD

        ) pixelIndices

-- entry point
let main [s] (width: i32)
             (height: i32)
             -- spheres and lights
             (numS: i32)
             (numL: i32)
             (sPositions: [s][3]f32)
             (sRadii: [s]f32)
             (sColors: [s][4]u8)
             -- return pixel color
             : [][4]u8 =
    -- combine data for render function
    let spheres = map3 (\p r c -> {pos = p, radius = r, color = c})
        sPositions sRadii sColors
    -- input checks
    let validInputs: bool = (length spheres) == (numS + numL)
    in if !validInputs
        then [[0,0,0,0]]
        -- render
        else render [width, height] numS numL spheres
