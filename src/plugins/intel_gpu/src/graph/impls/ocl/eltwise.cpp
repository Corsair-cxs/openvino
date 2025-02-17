// Copyright (C) 2018-2022 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include "eltwise_inst.h"
#include "primitive_base.hpp"
#include "impls/implementation_map.hpp"
#include "intel_gpu/runtime/error_handler.hpp"
#include "kernel_selector_helper.h"
#include "eltwise/eltwise_kernel_selector.h"
#include "eltwise/eltwise_kernel_base.h"
#include <vector>

namespace cldnn {
namespace ocl {

struct eltwise_impl : typed_primitive_impl_ocl<eltwise> {
    using parent = typed_primitive_impl_ocl<eltwise>;
    using parent::parent;
    using kernel_selector_t = kernel_selector::eltwise_kernel_selector;
    using kernel_params_t = std::pair<kernel_selector::eltwise_params, kernel_selector::eltwise_optional_params>;

    DECLARE_OBJECT_TYPE_SERIALIZATION

    std::unique_ptr<primitive_impl> clone() const override {
        return make_unique<eltwise_impl>(*this);
    }

protected:
    kernel_arguments_data get_arguments(const typed_primitive_inst<eltwise>& instance, int32_t split) const override {
        kernel_arguments_data args = parent::get_arguments(instance, split);
        return args;
    }

public:
    static kernel_params_t get_kernel_params(const kernel_impl_params& impl_param) {
        const auto& primitive = impl_param.typed_desc<eltwise>();
        auto inputs_count = primitive->input.size();

        auto params = get_default_params<kernel_selector::eltwise_params>(impl_param);
        auto optional_params = get_default_optional_params<kernel_selector::eltwise_optional_params>(impl_param.get_program());

        for (size_t i = 1; i < inputs_count; i++) {
            params.inputs.push_back(convert_data_tensor(impl_param.input_layouts[i]));
        }

        params.operations.push_back({{kernel_selector::eltwise_params::InputType::Buffer(0), kernel_selector::eltwise_params::InputType::Buffer(1)},
                                     convert_to_eltwise_mode(primitive->mode)});

        for (uint32_t i = 2; i < static_cast<uint32_t>(inputs_count); i++) {
            params.operations.push_back({{kernel_selector::eltwise_params::InputType::Intermediate(i - 2),
                                          kernel_selector::eltwise_params::InputType::Buffer(i)},
                                          convert_to_eltwise_mode(primitive->mode)});
        }

        if (primitive->mode == eltwise_mode::sum) {
            params.coefficients = primitive->coefficients;
        }

        for (size_t i = 0; i < params.inputs.size(); i++) {
            if (!params.inputs[i].SameDims(params.outputs[0])) {
                std::vector<int32_t> input_size = impl_param.input_layouts[i].get_tensor().raw.vector();
                std::vector<int32_t> output_size = impl_param.get_output_layout().get_tensor().raw.vector();
                bool broadcast = false;
                for (size_t d = 0; d < output_size.size(); d++) {
                    if (output_size[d] != 1 && input_size[d] == 1)
                        broadcast = true;
                }
                if (broadcast) {
                    params.broadcast = true;
                    break;
                } else {
                    params.layoutBased = true;
                    break;
                }
            }
        }

        // stride
        if (!primitive->stride.empty()) {
            const auto& stride = primitive->stride;
            params.stride.resize(stride.size());
            for (size_t i = 0; i < primitive->stride.size(); i++) {
                params.stride[i] = {(uint32_t)stride[i].spatial[0],
                                    (uint32_t)stride[i].spatial[1],
                                    (uint32_t)stride[i].spatial[2]};
            }
        }

        // check if strides are the same
        if (!params.stride.empty()) {
            const auto& stride = params.stride[0];
            for (size_t i = 1; i < params.stride.size(); i++) {
                if (stride.x != params.stride[i].x || stride.y != params.stride[i].y)
                    params.layoutBased = true;
            }
        } else if (!params.inputs[0].SameDimsSizes(params.inputs[1])) {
            params.broadcast = true;
        }

        // TODO [LOW PRECISION]: check if this parameter's really needed. Maybe data types are enough
        bool quantization = true;
        for (size_t i = 0; i < inputs_count; i++) {
            if (impl_param.input_layouts[i].data_type != data_types::u8 &&
                impl_param.input_layouts[i].data_type != data_types::i8) {
                quantization = false;
            }
        }
        params.int8_quantization = quantization;

        return {params, optional_params};
    }
};

namespace detail {

attach_eltwise_impl::attach_eltwise_impl() {
    implementation_map<eltwise>::add(impl_types::ocl, typed_primitive_impl_ocl<eltwise>::create<eltwise_impl>, {
        std::make_tuple(data_types::f32, format::yxfb),
        std::make_tuple(data_types::f16, format::yxfb),
        std::make_tuple(data_types::i8, format::yxfb),
        std::make_tuple(data_types::u8, format::yxfb),
        std::make_tuple(data_types::i32, format::yxfb),
        std::make_tuple(data_types::i64, format::yxfb),

        std::make_tuple(data_types::f32, format::bfyx),
        std::make_tuple(data_types::f16, format::bfyx),
        std::make_tuple(data_types::u8, format::bfyx),
        std::make_tuple(data_types::i8, format::bfyx),
        std::make_tuple(data_types::i32, format::bfyx),
        std::make_tuple(data_types::i64, format::bfyx),

        std::make_tuple(data_types::f32, format::byxf),
        std::make_tuple(data_types::f16, format::byxf),
        std::make_tuple(data_types::i8, format::byxf),
        std::make_tuple(data_types::u8, format::byxf),
        std::make_tuple(data_types::i32, format::byxf),
        std::make_tuple(data_types::i64, format::byxf),

        std::make_tuple(data_types::f16, format::b_fs_yx_fsv16),
        std::make_tuple(data_types::f32, format::b_fs_yx_fsv16),
        std::make_tuple(data_types::i8, format::b_fs_yx_fsv16),
        std::make_tuple(data_types::u8, format::b_fs_yx_fsv16),

        std::make_tuple(data_types::f32, format::bfzyx),
        std::make_tuple(data_types::f16, format::bfzyx),
        std::make_tuple(data_types::i8, format::bfzyx),
        std::make_tuple(data_types::u8, format::bfzyx),
        std::make_tuple(data_types::i32, format::bfzyx),
        std::make_tuple(data_types::i64, format::bfzyx),

        std::make_tuple(data_types::f32, format::bfwzyx),
        std::make_tuple(data_types::f16, format::bfwzyx),
        std::make_tuple(data_types::i8, format::bfwzyx),
        std::make_tuple(data_types::u8, format::bfwzyx),
        std::make_tuple(data_types::i32, format::bfwzyx),
        std::make_tuple(data_types::i64, format::bfwzyx),

        std::make_tuple(data_types::f32, format::b_fs_zyx_fsv16),
        std::make_tuple(data_types::f16, format::b_fs_zyx_fsv16),
        std::make_tuple(data_types::i8, format::b_fs_zyx_fsv16),
        std::make_tuple(data_types::u8, format::b_fs_zyx_fsv16),
        std::make_tuple(data_types::i32, format::b_fs_zyx_fsv16),
        std::make_tuple(data_types::i64, format::b_fs_zyx_fsv16),

        std::make_tuple(data_types::f32, format::bs_fs_zyx_bsv16_fsv16),
        std::make_tuple(data_types::f16, format::bs_fs_zyx_bsv16_fsv16),
        std::make_tuple(data_types::i8, format::bs_fs_zyx_bsv16_fsv16),
        std::make_tuple(data_types::i32, format::bs_fs_zyx_bsv16_fsv16),
        std::make_tuple(data_types::i64, format::bs_fs_zyx_bsv16_fsv16),

        std::make_tuple(data_types::f32, format::bs_fs_zyx_bsv16_fsv32),
        std::make_tuple(data_types::f16, format::bs_fs_zyx_bsv16_fsv32),
        std::make_tuple(data_types::i8, format::bs_fs_zyx_bsv16_fsv32),
        std::make_tuple(data_types::i32, format::bs_fs_zyx_bsv16_fsv32),
        std::make_tuple(data_types::i64, format::bs_fs_zyx_bsv16_fsv32),

        std::make_tuple(data_types::f32, format::bs_fs_yx_bsv16_fsv16),
        std::make_tuple(data_types::f16, format::bs_fs_yx_bsv16_fsv16),

        std::make_tuple(data_types::i8, format::b_fs_zyx_fsv2),
        std::make_tuple(data_types::u8, format::b_fs_zyx_fsv2),
        std::make_tuple(data_types::f16, format::b_fs_zyx_fsv2),
        std::make_tuple(data_types::f32, format::b_fs_zyx_fsv2),

        std::make_tuple(data_types::i8, format::bs_fs_zyx_bsv8_fsv2),
        std::make_tuple(data_types::u8, format::bs_fs_zyx_bsv8_fsv2),
        std::make_tuple(data_types::f16, format::bs_fs_zyx_bsv8_fsv2),
        std::make_tuple(data_types::f32, format::bs_fs_zyx_bsv8_fsv2),

        std::make_tuple(data_types::i8, format::bs_fs_zyx_bsv16_fsv2),
        std::make_tuple(data_types::u8, format::bs_fs_zyx_bsv16_fsv2),
        std::make_tuple(data_types::f16, format::bs_fs_zyx_bsv16_fsv2),
        std::make_tuple(data_types::f32, format::bs_fs_zyx_bsv16_fsv2),

        std::make_tuple(data_types::i8, format::b_fs_yx_fsv4),
        std::make_tuple(data_types::u8, format::b_fs_yx_fsv4),
        std::make_tuple(data_types::f32, format::b_fs_yx_fsv4),

        std::make_tuple(data_types::i8, format::b_fs_yx_fsv32),
        std::make_tuple(data_types::u8, format::b_fs_yx_fsv32),
        std::make_tuple(data_types::f32, format::b_fs_yx_fsv32),
        std::make_tuple(data_types::f16, format::b_fs_yx_fsv32),

        std::make_tuple(data_types::i8, format::b_fs_zyx_fsv32),
        std::make_tuple(data_types::u8, format::b_fs_zyx_fsv32),
        std::make_tuple(data_types::f32, format::b_fs_zyx_fsv32),
        std::make_tuple(data_types::f16, format::b_fs_zyx_fsv32),

        std::make_tuple(data_types::f16, format::fs_b_yx_fsv32),

        std::make_tuple(data_types::f32, format::bs_fs_yx_bsv32_fsv32),
        std::make_tuple(data_types::f16, format::bs_fs_yx_bsv32_fsv32),
        std::make_tuple(data_types::i8, format::bs_fs_yx_bsv32_fsv32),
        std::make_tuple(data_types::u8, format::bs_fs_yx_bsv32_fsv32),
        std::make_tuple(data_types::i32, format::bs_fs_yx_bsv32_fsv32),
        std::make_tuple(data_types::i64, format::bs_fs_yx_bsv32_fsv32),

        std::make_tuple(data_types::f32, format::bs_fs_yx_bsv32_fsv16),
        std::make_tuple(data_types::f16, format::bs_fs_yx_bsv32_fsv16),
        std::make_tuple(data_types::i8, format::bs_fs_yx_bsv32_fsv16),
        std::make_tuple(data_types::u8, format::bs_fs_yx_bsv32_fsv16),
        std::make_tuple(data_types::i32, format::bs_fs_yx_bsv32_fsv16),
        std::make_tuple(data_types::i64, format::bs_fs_yx_bsv32_fsv16),

        std::make_tuple(data_types::f32, format::bs_fs_yx_bsv16_fsv32),
        std::make_tuple(data_types::f16, format::bs_fs_yx_bsv16_fsv32),
        std::make_tuple(data_types::i8, format::bs_fs_yx_bsv16_fsv32),
        std::make_tuple(data_types::u8, format::bs_fs_yx_bsv16_fsv32),
        std::make_tuple(data_types::i32, format::bs_fs_yx_bsv16_fsv32),
        std::make_tuple(data_types::i64, format::bs_fs_yx_bsv16_fsv32),

        std::make_tuple(data_types::f32, format::bs_fs_yx_bsv4_fsv4),
        std::make_tuple(data_types::f16, format::bs_fs_yx_bsv4_fsv4),
        std::make_tuple(data_types::i8, format::bs_fs_yx_bsv4_fsv4),
        std::make_tuple(data_types::u8, format::bs_fs_yx_bsv4_fsv4),
        std::make_tuple(data_types::i32, format::bs_fs_yx_bsv4_fsv4),
        std::make_tuple(data_types::i64, format::bs_fs_yx_bsv4_fsv4),

        std::make_tuple(data_types::f32, format::bs_fs_yx_bsv8_fsv4),
        std::make_tuple(data_types::f16, format::bs_fs_yx_bsv8_fsv4),
        std::make_tuple(data_types::i8, format::bs_fs_yx_bsv8_fsv4),
        std::make_tuple(data_types::u8, format::bs_fs_yx_bsv8_fsv4),
        std::make_tuple(data_types::i32, format::bs_fs_yx_bsv8_fsv4),
        std::make_tuple(data_types::i64, format::bs_fs_yx_bsv8_fsv4),

        std::make_tuple(data_types::f32, format::bs_fs_yx_bsv8_fsv2),
        std::make_tuple(data_types::f16, format::bs_fs_yx_bsv8_fsv2),
        std::make_tuple(data_types::i8, format::bs_fs_yx_bsv8_fsv2),
        std::make_tuple(data_types::u8, format::bs_fs_yx_bsv8_fsv2),
        std::make_tuple(data_types::i32, format::bs_fs_yx_bsv8_fsv2),
        std::make_tuple(data_types::i64, format::bs_fs_yx_bsv8_fsv2),

        std::make_tuple(data_types::f32, format::bs_fs_yx_bsv4_fsv2),
        std::make_tuple(data_types::f16, format::bs_fs_yx_bsv4_fsv2),
        std::make_tuple(data_types::i8, format::bs_fs_yx_bsv4_fsv2),
        std::make_tuple(data_types::u8, format::bs_fs_yx_bsv4_fsv2),
        std::make_tuple(data_types::i32, format::bs_fs_yx_bsv4_fsv2),
        std::make_tuple(data_types::i64, format::bs_fs_yx_bsv4_fsv2),

        std::make_tuple(data_types::f32, format::bs_fs_zyx_bsv32_fsv32),
        std::make_tuple(data_types::f16, format::bs_fs_zyx_bsv32_fsv32),
        std::make_tuple(data_types::i8, format::bs_fs_zyx_bsv32_fsv32),
        std::make_tuple(data_types::u8, format::bs_fs_zyx_bsv32_fsv32),
        std::make_tuple(data_types::i32, format::bs_fs_zyx_bsv32_fsv32),
        std::make_tuple(data_types::i64, format::bs_fs_zyx_bsv32_fsv32),

        std::make_tuple(data_types::f32, format::bs_fs_zyx_bsv32_fsv16),
        std::make_tuple(data_types::f16, format::bs_fs_zyx_bsv32_fsv16),
        std::make_tuple(data_types::i8, format::bs_fs_zyx_bsv32_fsv16),
        std::make_tuple(data_types::u8, format::bs_fs_zyx_bsv32_fsv16),
        std::make_tuple(data_types::i32, format::bs_fs_zyx_bsv32_fsv16),
        std::make_tuple(data_types::i64, format::bs_fs_zyx_bsv32_fsv16),
    });
}

}  // namespace detail
}  // namespace ocl
}  // namespace cldnn

BIND_BINARY_BUFFER_WITH_TYPE(cldnn::ocl::eltwise_impl)
