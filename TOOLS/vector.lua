--Definitely Not LAPACK But Good Enough

--Subtraction
function vector_sub(lhs, rhs)
    return { lhs[1]-rhs[1], lhs[2]-rhs[2], lhs[3]-rhs[3] }
end
--Addition
function vector_add(lhs, rhs)
    return { lhs[1]+rhs[1], lhs[2]+rhs[2], lhs[3]+rhs[3] }
end
--Multiplication
function vector_mul(lhs, rhs)
    return { lhs[1]*rhs[1], lhs[2]*rhs[2], lhs[3]*rhs[3] }
end
--Determinant
function vector_det(lhs)
    return lhs[1] + lhs[2] + lhs[3]
end
--Dot product
function vector_dot(lhs, rhs)
    return vector_det(vector_mul(lhs, rhs))
end
--Cross product
function vector_cross(lhs,rhs)
    return {
        lhs[2]*rhs[3] - lhs[3]*rhs[2],
        lhs[3]*rhs[1] - lhs[1]*rhs[3],
        lhs[1]*rhs[2] - lhs[2]*rhs[1],
    }
end

--I think it's better to have this in Lua rather than C because the
--interpreter or compiler can optimize them out. Also need to see metatables.
