-- module types

local module type scalar = {
    type t
    val zero : t
    val add : t -> t -> t
    val sub : t -> t -> t
    val mul : t -> t -> t
    val div : t -> t -> t
}

-- module blueprints

local module linalg (M: scalar) = {
    open M

    let dot [n] (a: [n]t) (b: [n]t): t =
        reduce add zero (map2 mul a b)

    let matmul [n][p][m] (A: [n][p]t) (B: [p][m]t): [n][m]t =
        map (\ar -> map (\bc -> dot ar bc) (transpose B)) A
}

-- modules

module i32_l = linalg {
    type t = i32
    let zero: t = 0
    let add (x: t) (y: t): t = x + y
    let mul (x: t) (y: t): t = x * y
}

module f32_l = linalg {
    type t = f32
    let zero: t = 0
    let add (x: t) (y: t): t = x + y
    let sub (x: t) (y: t): t = x - y
    let mul (x: t) (y: t): t = x * y
    let div (x: t) (y: t): t = x / y
}

-- example main

--let main: [][]i32_linalg.t =
--    let A: [][]i32_linalg.t = [[1,1,1],[1,1,1]]
--    let B: [][]i32_linalg.t = [[1,1,1],[1,1,1],[1,1,1]]
--    in i32_linalg.matmul A B
