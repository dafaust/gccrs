// { dg-output "x:15\ny:20\n" }

extern "C" {
    fn printf(s: *const i8, ...);
}

enum Foo {
    A,
    B,
}

// FIXME: cloning some let statements inside the blocks when rearranging
// the match in HIR is causing SEGV.
// fn inspect(f: Foo, g: u8) {
//     match (f, g) {
//         (Foo::A, 1) => {
//             let a = "One A\n\0";
//             let b = a as *const str;
//             let c = b as *const i8;
//             printf (c);
//         }

//         (Foo::A, 2) => {
//             let a = "Two A\n\0";
//             let b = a as *const str;
//             let c = b as *const i8;
//             printf (c);
//         }

//         (Foo::B, 2) => {
//             let a = "To Be\n\0";
//             let b = a as *const str;
//             let c = b as *const i8;
//             printf (c);
//         }

//         _ => {
//             let a = "Or Not\n\0";
//             let b = a as *const str;
//             let c = b as *const i8;
//             printf (c);
//         }
//     }
// }

fn inspect(f: Foo, g: u8) -> i32 {
    match (f, g) {
        (Foo::A, 1) => {
            return 5;
        }

        (Foo::A, 2) => {
            return 10;
        }

        (Foo::B, 2) => {
            return 15;
        }

        _ => {
            return 20;
        }
    }
    return 25;
}

fn main () -> i32 {
    let x = inspect (Foo::B, 2);
    let y = inspect (Foo::B, 1);

    printf ("x:%d\n" as *const str as *const i8, x);
    printf ("y:%d\n" as *const str as *const i8, y);

    y - x - 5
}
