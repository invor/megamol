/*
 * AbstractVector.h
 *
 * Copyright (C) 2006 by Universitaet Stuttgart (VIS). Alle Rechte vorbehalten.
 * Copyright (C) 2005 by Christoph Mueller. Alle Rechte vorbehalten.
 */

#ifndef VISLIB_ABSTRACTVECTOR_H_INCLUDED
#define VISLIB_ABSTRACTVECTOR_H_INCLUDED
#if (_MSC_VER > 1000)
#pragma once
#endif /* (_MSC_VER > 1000) */


#include <limits>

#include "vislib/assert.h"
#include "vislib/OutOfRangeException.h"


namespace vislib {
namespace math {

    /**

     */
    template<class T, unsigned int D, class E, class S> class AbstractVector {

    public:

        /** Dtor. */
        ~AbstractVector(void);

        /**
         * Answer the dot product of this vector and 'rhs'.
         *
         * @param rhs The right hand side operand.
         *
         * @return The dot product of this vector and 'rhs'.
         */
        T Dot(const AbstractVector& rhs) const;

        /**
         * Answer whether the vector is a null vector.
         *
         * @return true, if the vector is a null vector, false otherwise.
         */
        bool IsNull(void) const;

        /**
         * Answer the length of the vector.
         *
         * @return The length of the vector.
         */
        T Length(void) const;

        /**
         * Answer the maximum norm of the vector.
         *
         * @return The maximum norm of the vector.
         */
        T MaxNorm(void) const;

        /**
         * Answer the euclidean norm (length) of the vector.
         *
         * @return The length of the vector.
         */
        inline T Norm(void) const {
            return this->Length();
        }

        /**
         * Normalise the vector.
         *
         * @return The OLD length of the vector.
         */
        T Normalise(void);

        /**
         * Directly access the internal pointer holding the vector components 
         * to the caller. The object remains owner of the memory returned.
         *
         * @return The vector components in an array.
         */
        inline T * PeekComponents(void) {
            return this->components;
        }

        /**
         * Directly access the internal pointer holding the vector components 
         * to the caller. The object remains owner of the memory returned.
         *
         * @return The vector components in an array.
         */
        inline const T *PeekComponents(void) const {
            return this->components;
        }

        /**
         * Assignment.
         *
         * @param rhs The right hand side operand.
         *
         * @return *this
         */
        AbstractVector& operator =(const AbstractVector& rhs);

        /**
         * Assigment for arbitrary vectors. A valid static_cast between T and Tp
         * is a precondition for instantiating this template.
         *
         * This operation does <b>not</b> create aliases. 
         *
         * If the two operands have different dimensions, the behaviour is as 
         * follows: If the left hand side operand has lower dimension, the 
         * highest (Dp - D) dimensions are discarded. If the left hand side
         * operand has higher dimension, the missing dimensions are filled with 
         * zero components.
         *
         * Subclasses must ensure that sufficient memory for the 'components' 
         * member has been allocated before calling this operator.
         *
         * @param rhs The right hand side operand.
         *
         * @return *this
         */
        template<class Tp, unsigned int Dp, class Ep, class Sp>
        AbstractVector& operator =(const AbstractVector<Tp, Dp, Ep, Sp>& rhs);

        /**
         * Test for equality. This operation uses the E function which the 
         * template has been instantiated for.
         *
         * @param rhs The right hand side operand.
         *
         * @param true, if 'rhs' and this vector are equal, false otherwise.
         */
        bool operator ==(const AbstractVector& rhs) const;

        /**
         * Test for equality of arbitrary vector types. This operation uses the
         * E equal functor of the left hand side operand. The Ep functor of the
         * right hand side operand is ignored. Note that vectors with different
         * dimensions are never equal.
         *
         * @param rhs The right hand side operand.
         *
         * @param true, if 'rhs' and this vector are equal, false otherwise.
         */
        template<class Tp, unsigned int Dp, class Ep, class Sp>
        bool operator ==(const AbstractVector<Tp, Dp, Ep, Sp>& rhs) const;

        /**
         * Test for inequality.
         *
         * @param rhs The right hand side operand.
         *
         * @param true, if 'rhs' and this vector are not equal, false otherwise.
         */
        inline bool operator !=(const AbstractVector& rhs) const {
            return !(*this == rhs);
        }

        /**
         * Test for inequality of arbitrary vectors. The operator == for further
         * details.
         *
         * @param rhs The right hand side operand.
         *
         * @param true, if 'rhs' and this vector are not equal, false otherwise.
         */
        template<class Tp, unsigned int Dp, class Ep, class Sp>
        inline bool operator !=(
                const AbstractVector<Tp, Dp, Ep, Sp>& rhs) const {
            return !(*this == rhs);
        }

        /**
         * Negate the vector.
         *
         * @return The negated version of this vector.
         */
        AbstractVector operator -(void) const;

        /**
         * Answer the sum of this vector and 'rhs'.
         *
         * @param rhs The right hand side operand.
         *
         * @return The sum of this and 'rhs'.
         */
        AbstractVector operator +(const AbstractVector& rhs) const;

        /**
         * Add 'rhs' to this vector and answer the sum.
         *
         * @param rhs The right hand side operand.
         *
         * @return The sum of this and 'rhs'.
         */
        AbstractVector& operator +=(const AbstractVector& rhs);

        /**
         * Answer the difference between this vector and 'rhs'.
         *
         * @param rhs The right hand side operand.
         *
         * @return The difference between this and 'rhs'.
         */
        AbstractVector operator -(const AbstractVector& rhs) const;

        /**
         * Subtract 'rhs' from this vector and answer the difference.
         *
         * @param rhs The right hand side operand.
         *
         * @return The difference between this and 'rhs'.
         */
        AbstractVector& operator -=(const AbstractVector& rhs);

        /**
         * Scalar multiplication.
         *
         * @param rhs The right hand side operand.
         *
         * @return The result of the scalar multiplication.
         */
        AbstractVector operator *(const T rhs) const;

        /**
         * Scalar multiplication assignment operator.
         *
         * @param rhs The right hand side operand.
         *
         * @return The result of the scalar multiplication.
         */
        AbstractVector& operator *=(const T rhs);

        /**
         * Scalar division operator.
         *
         * @param rhs The right hand side operand.
         *
         * @return The result of the scalar division.
         */
        AbstractVector operator /(const T rhs) const;

        /**
         * Scalar division assignment operator.
         *
         * @param rhs The right hand side operand.
         *
         * @return The result of the scalar division.
         */
        AbstractVector& operator /=(const T rhs);

        /**
         * Performs a component-wise multiplication.
         *
         * @param rhs The right hand side operand.
         *
         * @return The product of this and rhs.
         */
        AbstractVector operator *(const AbstractVector& rhs) const;

        /**
         * Multiplies 'rhs' component-wise with this vector and returns
         * the result.
         *
         * @param rhs The right hand side operand.
         *
         * @return The product of this and rhs
         */
        AbstractVector& operator *=(const AbstractVector& rhs);

        /**
         * Component access.
         *
         * @param i The index of the requested component, which must be within
         *          [0, D - 1].
         *
         * @return A reference on the 'i'th component.
         *
         * @throws OutOfRangeException, If 'i' is not within [0, D[.
         */
        T& operator [](const int i);

        /**
         * Component access.
         *
         * @param i The index of the requested component, which must be within
         *          [0, D - 1].
         *
         * @return A reference on the 'i'th component.
         *
         * @throws OutOfRangeException, If 'i' is not within [0, D[.
         */
        const T& operator [](const int i) const;

    protected:

        /**
         * Disallow instances of this class. This ctor does nothing!
         */
        inline AbstractVector(void) {};

        /** 
         * The AbstractVector components. This can be a ShallowVectorStorage or
         * DeepVectorStorage instantiation.
         */
        S components;
    };

    /*
     * AbstractVector<T, D, E, S>::~AbstractVector
     */
    template<class T, unsigned int D, class E, class S>
    AbstractVector<T, D, E, S>::~AbstractVector(void) {
    }


    /*
     * AbstractVector<T, D, E, S>::Dot
     */
    template<class T, unsigned int D, class E, class S>
    T AbstractVector<T, D, E, S>::Dot(const AbstractVector& rhs) const {
        T retval = static_cast<T>(0);

        for (unsigned int d = 0; d < D; d++) {
            retval += this->components[d] * rhs.components[d];
        }

        return retval;
    }


    /*
     * AbstractVector<T, D, E, S>::IsNull
     */
    template<class T, unsigned int D, class E,  class S>
    bool AbstractVector<T, D, E, S>::IsNull(void) const {
        for (unsigned int d = 0; d < D; d++) {
            if (!E()(this->components[d], static_cast<T>(0))) {
                return false;
            }
        }
        /* No non-null value found. */

        return true;
    }


    /*
     * AbstractVector<T, D, E, S>::Length
     */
    template<class T, unsigned int D, class E, class S>
    T AbstractVector<T, D, E, S>::Length(void) const {
        T retval = static_cast<T>(0);

        for (unsigned int d = 0; d < D; d++) {
            retval += Sqr(this->components[d]);
        }

        return Sqrt(retval);
    }


    /*
     * AbstractVector<T, D, E, S>::MaxNorm
     */
    template<class T, unsigned int D, class E, class S>
    T AbstractVector<T, D, E, S>::MaxNorm(void) const {
#ifdef _MSC_VER
#pragma push_macro("min")
#pragma push_macro("max")
#undef min
#undef max
#endif /* _MSC_VER */
        T retval = std::numeric_limits<T>::is_integer 
            ? std::numeric_limits<T>::min() : -std::numeric_limits<T>::max();
#ifdef _MSC_VER
#define min
#define max
#pragma pop_macro("min")
#pragma pop_macro("max")
#endif /* _MSC_VER */

        for (unsigned int d = 0; d < D; d++) {
            if (this->components[d] > retval) {
                retval = this->components[d];
            }
        }

        return retval;
    }


    /*
     * AbstractVector<T, D, E, S>::Normalise
     */
    template<class T, unsigned int D, class E, class S>
    T AbstractVector<T, D, E, S>::Normalise(void) {
        T length = this->Length();

        if (length != static_cast<T>(0)) {
            for (unsigned int d = 0; d < D; d++) {
                this->components[d] /= length;
            }

        } else {
            for (unsigned int d = 0; d < D; d++) {
                this->components[d] = static_cast<T>(0);
            }
        }

        return length;
    }


    /*
     * AbstractVector<T, D, E, S>::operator =
     */
    template<class T, unsigned int D, class E, class S>
    AbstractVector<T, D, E, S>& AbstractVector<T, D, E, S>::operator =(
            const AbstractVector& rhs) {
        if (this != &rhs) {
            ::memcpy(this->components, rhs.components, D * sizeof(T));
        }

        return *this;
    }


    /*
     * AbstractVector<T, D, E, S>::operator =
     */
    template<class T, unsigned int D, class E, class S>
    template<class Tp, unsigned int Dp, class Ep, class Sp>
    AbstractVector<T, D, E, S>& AbstractVector<T, D, E, S>::operator =(
            const AbstractVector<Tp, Dp, Ep, Sp>& rhs) {
        if (static_cast<void *>(this) != static_cast<const void *>(&rhs)) {
            for (unsigned int d = 0; (d < D) && (d < Dp); d++) {
                this->components[d] = static_cast<T>(rhs[d]);
            }
            for (unsigned int d = Dp; d < D; d++) {
                this->components[d] = static_cast<T>(0);
            }            
        }

        return *this;
    }


    /*
     * AbstractVector<T, D, E, S>::operator ==
     */
    template<class T, unsigned int D, class E,  class S>
    bool AbstractVector<T, D, E, S>::operator ==(
            const AbstractVector& rhs) const {
        for (unsigned int d = 0; d < D; d++) {
            if (!E()(this->components[d], rhs.components[d])) {
                return false;
            }
        }

        return true;
    }


    /*
     * vislib::math::AbstractVector<T, D, E, S>::operator ==
     */
    template<class T, unsigned int D, class E,  class S>
    template<class Tp, unsigned int Dp, class Ep, class Sp>
    bool AbstractVector<T, D, E, S>::operator ==(
            const AbstractVector<Tp, Dp, Ep, Sp>& rhs) const {
        if (D != Dp) {
            return false;
        }

        for (unsigned int d = 0; d < D; d++) {
            if (!E()(this->components[d], static_cast<T>(components[d]))) {
                return false;
            }
        }

        return true;
    }


    /*
     * AbstractVector<T, D, E, S>::operator -
     */
    template<class T, unsigned int D, class E, class S>
    AbstractVector<T, D, E, S> AbstractVector<T, D, E, S>::operator -(
            void) const {
        AbstractVector<T, D, E, S> retval;

        for (unsigned int d = 0; d < D; d++) {
            retval.components[d] = -this->components[d];
        }

        return retval;
    }


    /*
     * AbstractVector<T, D, E, S>::operator +
     */
    template<class T, unsigned int D, class E, class S>
    AbstractVector<T, D, E, S> AbstractVector<T, D, E, S>::operator +(
            const AbstractVector& rhs) const {
        AbstractVector<T, D, E, S> retval;

        for (unsigned int d = 0; d < D; d++) {
            retval.components[d] = this->components[d] + rhs.components[d];
        }

        return retval;
    }


    /*
     * AbstractVector<T, D, E, S>::operator +=
     */
    template<class T, unsigned int D, class E, class S>
    AbstractVector<T, D, E, S>& AbstractVector<T, D, E, S>::operator +=(
            const AbstractVector& rhs) {

        for (unsigned int d = 0; d < D; d++) {
            this->components[d] += rhs.components[d];
        }

        return *this;
    }


    /*
     * AbstractVector<T, D, E, S>::operator -
     */
    template<class T, unsigned int D, class E,  class S>
    AbstractVector<T, D, E, S> AbstractVector<T, D, E, S>::operator -(
            const AbstractVector& rhs) const {
        AbstractVector<T, D, E, S> retval;

        for (unsigned int d = 0; d < D; d++) {
            retval.components[d] = this->components[d] - rhs.components[d];
        }

        return retval;
    }


    /*
     * AbstractVector<T, D, E, S>::operator -=
     */
    template<class T, unsigned int D, class E, class S>
    AbstractVector<T, D, E, S>& AbstractVector<T, D, E, S>::operator -=(
            const AbstractVector& rhs) {

        for (unsigned int d = 0; d < D; d++) {
            this->components[d] -= rhs.components[d];
        }
       
        return *this;
    }


    /*
     * AbstractVector<T, D, E, S>::operator *
     */
    template<class T, unsigned int D, class E, class S>
    AbstractVector<T, D, E, S> AbstractVector<T, D, E, S>::operator *(
            const T rhs) const {
        AbstractVector<T, D, E, S> retval;

        for (unsigned int d = 0; d < D; d++) {
            retval.components[d] = this->components[d] * rhs;
        }

        return retval;
    }


    /*
     * AbstractVector<T, D, E, S>::operator *=
     */
    template<class T, unsigned int D, class E, class S>
    AbstractVector<T, D, E, S>& AbstractVector<T, D, E, S>::operator *=(
            const T rhs) {
        for (unsigned int d = 0; d < D; d++) {
            this->components[d] *= rhs;
        }

        return *this;
    }


    /*
     * AbstractVector<T, D, E, S>::operator /
     */
    template<class T, unsigned int D, class E, class S>
    AbstractVector<T, D, E, S> AbstractVector<T, D, E, S>::operator /(
            const T rhs) const {
        AbstractVector<T, D, E, S> retval;

        for (unsigned int d = 0; d < D; d++) {
            retval.components[d] = this->components[d] / rhs;
        }

        return retval;
    }


    /*
     * AbstractVector<T, D, E, S>::operator /=
     */
    template<class T, unsigned int D, class E, class S>
    AbstractVector<T, D, E, S>& AbstractVector<T, D, E, S>::operator /=(
            const T rhs) {
        for (unsigned int d = 0; d < D; d++) {
            this->components[d] /= rhs;
        }

        return *this;
    }


    /*
     * AbstractVector<T, D, E, S>::operator *
     */
    template<class T, unsigned int D, class E,  class S>
    AbstractVector<T, D, E, S> AbstractVector<T, D, E, S>::operator *(
            const AbstractVector& rhs) const {
        AbstractVector<T, D, E, S> retval;

        for (unsigned int d = 0; d < D; d++) {
            retval.components[d] = this->components[d] * rhs.components[d];
        }

        return retval;
    }


    /*
     * AbstractVector<T, D, E, S>::operator *=
     */
    template<class T, unsigned int D, class E, class S>
    AbstractVector<T, D, E, S>& AbstractVector<T, D, E, S>::operator *=(
            const AbstractVector& rhs) {

        for (unsigned int d = 0; d < D; d++) {
            this->components[d] *= rhs.components[i];
        }

        return *this;
    }


    /*
     * AbstractVector<T, D, E, S>::operator []
     */
    template<class T, unsigned int D, class E, class S>
    T& AbstractVector<T, D, E, S>::operator [](const int i) {
        if ((i >= 0) && (i < static_cast<int>(D))) {
            return this->components[i];
        } else {
            throw OutOfRangeException(i, 0, D - 1, __FILE__, __LINE__);
        }
    }


    /*
     * AbstractVector<T, D, E, S>::operator []
     */
    template<class T, unsigned int D, class E, class S>
    const T& AbstractVector<T, D, E, S>::operator [](const int i) const {
        if ((i >= 0) && (i < static_cast<int>(D))) {
            return this->components[i];
        } else {
            throw OutOfRangeException(i, 0, D - 1, __FILE__, __LINE__);
        }
    }


} /* end namespace math */
} /* end namespace vislib */

#endif /* VISLIB_ABSTRACTVECTOR_H_INCLUDED */
