// This file is part of OpenCV project.
// It is subject to the license terms in the LICENSE file found in the top-level directory
// of this distribution and at http://opencv.org/license.html.
//
// Copyright (C) 2018-2019 Intel Corporation


#ifndef OPENCV_GAPI_GCPUKERNEL_HPP
#define OPENCV_GAPI_GCPUKERNEL_HPP

#include <functional>
#include <unordered_map>
#include <utility>
#include <vector>

#include <opencv2/core/mat.hpp>
#include <opencv2/gapi/gcommon.hpp>
#include <opencv2/gapi/gkernel.hpp>
#include <opencv2/gapi/garg.hpp>
#include <opencv2/gapi/util/compiler_hints.hpp> //suppress_unused_warning
#include <opencv2/gapi/util/util.hpp>

// FIXME: namespace scheme for backends?
namespace cv {

namespace gimpl
{
    // Forward-declare an internal class
    class GCPUExecutable;

    namespace render
    {
    namespace ocv
    {
        class GRenderExecutable;
    }
    }
} // namespace gimpl

namespace gapi
{
namespace cpu
{
    /**
     * \addtogroup gapi_std_backends
     * @{
     *
     * @brief G-API backends available in this OpenCV version
     *
     * G-API backends play a corner stone role in G-API execution
     * stack. Every backend is hardware-oriented and thus can run its
     * kernels efficiently on the target platform.
     *
     * Backends are usually "black boxes" for G-API users -- on the API
     * side, all backends are represented as different objects of the
     * same class cv::gapi::GBackend.
     * User can manipulate with backends by specifying which kernels to use.
     *
     * @sa @ref gapi_hld
     */

    /**
     * @brief Get a reference to CPU (OpenCV) backend.
     *
     * This is the default backend in G-API at the moment, providing
     * broader functional coverage but losing some graph model
     * advantages. Provided mostly for reference and prototyping
     * purposes.
     *
     * @sa gapi_std_backends
     */
    GAPI_EXPORTS cv::gapi::GBackend backend();
    /** @} */

    class GOCVFunctor;

    //! @cond IGNORED
    template<typename K, typename Callable>
    GOCVFunctor ocv_kernel(const Callable& c);

    template<typename K, typename Callable>
    GOCVFunctor ocv_kernel(Callable& c);
    //! @endcond

} // namespace cpu
} // namespace gapi

// Represents arguments which are passed to a wrapped CPU function
// FIXME: put into detail?
class GAPI_EXPORTS GCPUContext
{
public:
    // Generic accessor API
    template<typename T>
    const T& inArg(int input) { return m_args.at(input).get<T>(); }

    // Syntax sugar
    const cv::Mat&   inMat(int input);
    cv::Mat&         outMatR(int output); // FIXME: Avoid cv::Mat m = ctx.outMatR()

    const cv::Scalar& inVal(int input);
    cv::Scalar& outValR(int output); // FIXME: Avoid cv::Scalar s = ctx.outValR()
    template<typename T> std::vector<T>& outVecR(int output) // FIXME: the same issue
    {
        return outVecRef(output).wref<T>();
    }
    template<typename T> T& outOpaqueR(int output) // FIXME: the same issue
    {
        return outOpaqueRef(output).wref<T>();
    }

protected:
    detail::VectorRef& outVecRef(int output);
    detail::OpaqueRef& outOpaqueRef(int output);

    std::vector<GArg> m_args;

    //FIXME: avoid conversion of arguments from internal representation to OpenCV one on each call
    //to OCV kernel. (This can be achieved by a two single time conversions in GCPUExecutable::run,
    //once on enter for input and output arguments, and once before return for output arguments only
    std::unordered_map<std::size_t, GRunArgP> m_results;

    friend class gimpl::GCPUExecutable;
    friend class gimpl::render::ocv::GRenderExecutable;
};

class GAPI_EXPORTS GCPUKernel
{
public:
    // This function is kernel's execution entry point (does the processing work)
    using F = std::function<void(GCPUContext &)>;

    GCPUKernel();
    explicit GCPUKernel(const F& f);

    void apply(GCPUContext &ctx);

protected:
    F m_f;
};

// FIXME: This is an ugly ad-hoc implementation. TODO: refactor

namespace detail
{
template<class T> struct get_in;
template<> struct get_in<cv::GMat>
{
    static cv::Mat    get(GCPUContext &ctx, int idx) { return ctx.inMat(idx); }
};
template<> struct get_in<cv::GMatP>
{
    static cv::Mat    get(GCPUContext &ctx, int idx) { return get_in<cv::GMat>::get(ctx, idx); }
};
template<> struct get_in<cv::GFrame>
{
    static cv::Mat    get(GCPUContext &ctx, int idx) { return get_in<cv::GMat>::get(ctx, idx); }
};
template<> struct get_in<cv::GScalar>
{
    static cv::Scalar get(GCPUContext &ctx, int idx) { return ctx.inVal(idx); }
};
template<typename U> struct get_in<cv::GArray<U> >
{
    static const std::vector<U>& get(GCPUContext &ctx, int idx) { return ctx.inArg<VectorRef>(idx).rref<U>(); }
};
template<typename U> struct get_in<cv::GOpaque<U> >
{
    static const U& get(GCPUContext &ctx, int idx) { return ctx.inArg<OpaqueRef>(idx).rref<U>(); }
};

//FIXME(dm): GArray<Mat>/GArray<GMat> conversion should be done more gracefully in the system
template<> struct get_in<cv::GArray<cv::GMat> >: public get_in<cv::GArray<cv::Mat> >
{
};

//FIXME(dm): GArray<Scalar>/GArray<GScalar> conversion should be done more gracefully in the system
template<> struct get_in<cv::GArray<cv::GScalar> >: public get_in<cv::GArray<cv::Scalar> >
{
};

//FIXME(dm): GOpaque<Mat>/GOpaque<GMat> conversion should be done more gracefully in the system
template<> struct get_in<cv::GOpaque<cv::GMat> >: public get_in<cv::GOpaque<cv::Mat> >
{
};

//FIXME(dm): GOpaque<Scalar>/GOpaque<GScalar> conversion should be done more gracefully in the system
template<> struct get_in<cv::GOpaque<cv::GScalar> >: public get_in<cv::GOpaque<cv::Mat> >
{
};

template<class T> struct get_in
{
    static T get(GCPUContext &ctx, int idx) { return ctx.inArg<T>(idx); }
};

struct tracked_cv_mat{
    tracked_cv_mat(cv::Mat& m) : r{m}, original_data{m.data} {}
    cv::Mat r;
    uchar* original_data;

    operator cv::Mat& (){ return r;}
    void validate() const{
        if (r.data != original_data)
        {
            util::throw_error
                (std::logic_error
                 ("OpenCV kernel output parameter was reallocated. \n"
                  "Incorrect meta data was provided ?"));
        }
    }
};

template<typename... Outputs>
void postprocess(Outputs&... outs)
{
    struct
    {
        void operator()(tracked_cv_mat* bm) { bm->validate();  }
        void operator()(...)                {                  }

    } validate;
    //dummy array to unfold parameter pack
    int dummy[] = { 0, (validate(&outs), 0)... };
    cv::util::suppress_unused_warning(dummy);
}

template<class T> struct get_out;
template<> struct get_out<cv::GMat>
{
    static tracked_cv_mat get(GCPUContext &ctx, int idx)
    {
        auto& r = ctx.outMatR(idx);
        return {r};
    }
};
template<> struct get_out<cv::GMatP>
{
    static tracked_cv_mat get(GCPUContext &ctx, int idx)
    {
        return get_out<cv::GMat>::get(ctx, idx);
    }
};
template<> struct get_out<cv::GScalar>
{
    static cv::Scalar& get(GCPUContext &ctx, int idx)
    {
        return ctx.outValR(idx);
    }
};
template<typename U> struct get_out<cv::GArray<U>>
{
    static std::vector<U>& get(GCPUContext &ctx, int idx)
    {
        return ctx.outVecR<U>(idx);
    }
};
template<typename U> struct get_out<cv::GOpaque<U>>
{
    static U& get(GCPUContext &ctx, int idx)
    {
        return ctx.outOpaqueR<U>(idx);
    }
};

template<typename, typename, typename>
struct OCVCallHelper;

// FIXME: probably can be simplified with std::apply or analogue.
template<typename Impl, typename... Ins, typename... Outs>
struct OCVCallHelper<Impl, std::tuple<Ins...>, std::tuple<Outs...> >
{
    template<typename... Inputs>
    struct call_and_postprocess
    {
        template<typename... Outputs>
        static void call(Inputs&&... ins, Outputs&&... outs)
        {
            //not using a std::forward on outs is deliberate in order to
            //cause compilation error, by trying to bind rvalue references to lvalue references
            Impl::run(std::forward<Inputs>(ins)..., outs...);
            postprocess(outs...);
        }

        template<typename... Outputs>
        static void call(Impl& impl, Inputs&&... ins, Outputs&&... outs)
        {
            impl(std::forward<Inputs>(ins)..., outs...);
        }
    };

    template<int... IIs, int... OIs>
    static void call_impl(GCPUContext &ctx, detail::Seq<IIs...>, detail::Seq<OIs...>)
    {
        //Make sure that OpenCV kernels do not reallocate memory for output parameters
        //by comparing it's state (data ptr) before and after the call.
        //This is done by converting each output Mat into tracked_cv_mat object, and binding
        //them to parameters of ad-hoc function
        //Convert own::Scalar to cv::Scalar before call kernel and run kernel
        //convert cv::Scalar to own::Scalar after call kernel and write back results
        call_and_postprocess<decltype(get_in<Ins>::get(ctx, IIs))...>
                                      ::call(get_in<Ins>::get(ctx, IIs)...,
                                             get_out<Outs>::get(ctx, OIs)...);
    }

    template<int... IIs, int... OIs>
    static void call_impl(cv::GCPUContext &ctx, Impl& impl, detail::Seq<IIs...>, detail::Seq<OIs...>)
    {
        call_and_postprocess<decltype(cv::detail::get_in<Ins>::get(ctx, IIs))...>
                                      ::call(impl, cv::detail::get_in<Ins>::get(ctx, IIs)...,
                                                   cv::detail::get_out<Outs>::get(ctx, OIs)...);
    }

    static void call(GCPUContext &ctx)
    {
        call_impl(ctx,
                  typename detail::MkSeq<sizeof...(Ins)>::type(),
                  typename detail::MkSeq<sizeof...(Outs)>::type());
    }

    // NB: Same as call but calling the object
    // This necessary for kernel implementations that have a state
    // and are represented as an object
    static void callFunctor(cv::GCPUContext &ctx, Impl& impl)
    {
        call_impl(ctx, impl,
                  typename detail::MkSeq<sizeof...(Ins)>::type(),
                  typename detail::MkSeq<sizeof...(Outs)>::type());
    }
};

} // namespace detail

template<class Impl, class K>
class GCPUKernelImpl: public cv::detail::OCVCallHelper<Impl, typename K::InArgs, typename K::OutArgs>,
                      public cv::detail::KernelTag
{
    using P = detail::OCVCallHelper<Impl, typename K::InArgs, typename K::OutArgs>;

public:
    using API = K;

    static cv::gapi::GBackend backend()  { return cv::gapi::cpu::backend(); }
    static cv::GCPUKernel     kernel()   { return GCPUKernel(&P::call);     }
};

#define GAPI_OCV_KERNEL(Name, API) struct Name: public cv::GCPUKernelImpl<Name, API>

class gapi::cpu::GOCVFunctor : public gapi::GFunctor
{
public:
    using Impl = std::function<void(GCPUContext &)>;

    GOCVFunctor(const char* id, const Impl& impl)
        : gapi::GFunctor(id), impl_{GCPUKernel(impl)}
    {
    }

    GKernelImpl    impl()    const override { return impl_;                }
    gapi::GBackend backend() const override { return gapi::cpu::backend(); }

private:
    GKernelImpl impl_;
};

//! @cond IGNORED
template<typename K, typename Callable>
gapi::cpu::GOCVFunctor gapi::cpu::ocv_kernel(Callable& c)
{
    using P = detail::OCVCallHelper<Callable, typename K::InArgs, typename K::OutArgs>;
    return GOCVFunctor(K::id(), std::bind(&P::callFunctor, std::placeholders::_1, std::ref(c)));
}

template<typename K, typename Callable>
gapi::cpu::GOCVFunctor gapi::cpu::ocv_kernel(const Callable& c)
{
    using P = detail::OCVCallHelper<Callable, typename K::InArgs, typename K::OutArgs>;
    return GOCVFunctor(K::id(), std::bind(&P::callFunctor, std::placeholders::_1, c));
}
//! @endcond

} // namespace cv

#endif // OPENCV_GAPI_GCPUKERNEL_HPP
