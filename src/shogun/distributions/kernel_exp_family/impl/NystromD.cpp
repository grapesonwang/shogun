/*
 * Copyright (c) The Shogun Machine Learning Toolbox
 * Written (w) 2016 Heiko Strathmann
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation are those
 * of the authors and should not be interpreted as representing official policies,
 * either expressed or implied, of the Shogun Development Team.
 */

#include <shogun/lib/config.h>
#include <shogun/lib/SGMatrix.h>
#include <shogun/lib/SGVector.h>
#include <shogun/mathematics/eigen3.h>
#include <shogun/mathematics/Math.h>

#include "kernel/Base.h"
#include "NystromD.h"

#include <vector>
#include <set>
#include <algorithm>




using namespace shogun;
using namespace shogun::kernel_exp_family_impl;
using namespace Eigen;

NystromD::NystromD(SGMatrix<float64_t> data, SGMatrix<bool> basis_mask,
		std::shared_ptr<kernel::Base> kernel, float64_t lambda,
		float64_t lambda_l2)
		: Nystrom(data, data, kernel, lambda, lambda_l2, false)
{
	auto N = data.num_cols;

	// potentially subsample data and basis mask if certain points are unused
	SGMatrix<float64_t> basis;
	auto basis_point_inds = get_basis_point_inds(basis_inds_from_mask(basis_mask));
	if ((index_t)basis_point_inds.size() == N)
		basis=data;
	else
	{
		SGVector<index_t> wrap = SGVector<index_t>(basis_point_inds.data(),
				basis_point_inds.size(), false);
		SG_SINFO("Subsampling data as basis as some points are unused.\n");
		basis = subsample_matrix_cols(wrap, data);
		basis_mask = subsample_matrix_cols(wrap, basis_mask);
	}

	SG_SINFO("Using %d of N=%d user provided data points as basis points.\n",
			basis.num_cols, N);
	set_basis_inds_from_mask(basis_mask);
	set_basis_and_data(basis, data);
}

NystromD::NystromD(SGMatrix<float64_t> data, SGMatrix<float64_t> basis,
		SGMatrix<bool> basis_mask, std::shared_ptr<kernel::Base> kernel,
		float64_t lambda, float64_t lambda_l2)
		: Nystrom(data, basis, kernel, lambda, lambda_l2, true)
{
	set_basis_inds_from_mask(basis_mask);
}

void NystromD::set_basis_inds_from_mask(const SGMatrix<bool>& basis_mask)
{
	m_basis_inds = basis_inds_from_mask(basis_mask);

	// compute and potentially warn about unused basis points
	auto basis_point_inds = get_basis_point_inds(m_basis_inds);
	auto N = basis_mask.num_cols;
	std::vector<index_t> all_inds;
	for (index_t i=0; i<N; i++)
		all_inds.push_back(i);
	std::vector<index_t> unused;
	set_difference(	all_inds.begin(), all_inds.end(),
					basis_point_inds.begin(), basis_point_inds.end(),
					std::inserter(unused, unused.end()));

	for (size_t i=0; i<unused.size(); i++)
	{
		SG_SWARNING("Using zero components of basis point %d.\n", unused[i]);
	}

	SG_SINFO("Using %d of %dx%d=%d possible basis components.\n",
			m_basis_inds.size(), basis_mask.num_rows, basis_mask.num_cols,
			basis_mask.size());
}

SGVector<index_t> NystromD::basis_inds_from_mask(const SGMatrix<bool>& basis_mask) const
{
	std::vector<index_t> basis_inds_dynamic;
	int64_t num_mask_elements = (int64_t)basis_mask.num_rows*(int64_t)basis_mask.num_cols;
	for (auto i=0; i<num_mask_elements; i++)
	{
		if (basis_mask.matrix[i])
			basis_inds_dynamic.push_back(i);
	}

	// TODO make SGVector constructor from std::vector
	SGVector<index_t>basis_inds(basis_inds_dynamic.size());
	memcpy(basis_inds.vector, basis_inds_dynamic.data(),
			basis_inds_dynamic.size() * sizeof(index_t));

	// for linear memory traversals
	CMath::qsort(basis_inds);

	return basis_inds;
}

std::vector<index_t> NystromD::get_basis_point_inds(const SGVector<index_t>& basis_inds) const
{
	std::set<index_t> set;
	auto D = get_num_dimensions();

	for (auto i=0; i<basis_inds.vlen; i++)
	{
		auto ai = idx_to_ai(basis_inds[i], D);
		set.insert(ai.first);
	}

	std::vector<index_t> basis_point_inds;
	std::copy(set.begin(), set.end(), std::back_inserter(basis_point_inds));

	return basis_point_inds;
}

index_t NystromD::get_system_size() const
{
	return m_basis_inds.vlen;
}

SGVector<float64_t> NystromD::compute_h() const
{
//	SG_SWARNING("TODO: dont compute all and then sub-sample.\n");
//
//	auto h_full = Nystrom::compute_h();
//
//	auto system_size = get_system_size();
//	SGVector<float64_t> h(system_size);
//
//	// subsample vector entries
//	for (auto i=0; i<system_size; i++)
//		h[i] = h_full[m_basis_inds[i]];
//
//	return h;
	// WIP
	auto D = get_num_dimensions();
	auto N_basis = get_num_basis();
	auto N_data = get_num_data();
	auto system_size = N_basis*D;
	auto ND = N_data*D;

	SGVector<float64_t> h(system_size);
	h.zero();

#pragma omp parallel for
	for (auto idx_k=0; idx_k<system_size; idx_k++)
	{
		auto ai = idx_to_ai(m_basis_inds[idx_k], D);
		auto a = ai.first;
		auto i = ai.second;

		for (auto b=0; b<ND; b++)
		{
			for (auto idx_l=0; idx_l<system_size; idx_l++)
			{
				auto temp = idx_to_ai(m_basis_inds[idx_l], D);
				if (temp.first!=b)
					continue;

				auto j=temp.second;
				h[idx_k] += m_kernel->dx_dy_dy_component(a, b, i, j);
			}
		}
	}
//	for (auto idx_a=0; idx_a<N_basis; idx_a++)
//		for (auto idx_b=0; idx_b<N_data; idx_b++)
//		{
//			SGMatrix<float64_t> temp = m_kernel->dx_dy_dy(idx_a, idx_b);
//			eigen_h.segment(idx_a*D, D) += Map<MatrixXd>(temp.matrix, D,D).colwise().sum();
//		}
	h.scale(1.0 / N_data);

	return h;
}

SGMatrix<float64_t> NystromD::compute_G_mn() const
{
	auto D = get_num_dimensions();
	auto system_size = get_system_size();
	auto N = get_num_data();
	auto ND = N*D;

	SGMatrix<float64_t> G_mn(system_size, ND);

#pragma omp parallel for
	for (auto idx_l=0; idx_l<ND; idx_l	++)
	{
		auto ai = idx_to_ai(idx_l, D);
		auto a = ai.first;
		auto i = ai.second;

		for (auto idx_k=0; idx_k<system_size; idx_k++)
		{
			auto bj = idx_to_ai(m_basis_inds[idx_k], D);
			auto b = bj.first;
			auto j = bj.second;

			G_mn(idx_k,idx_l) = m_kernel->dx_dy_component(b, a, j, i);
		}
	}

	return G_mn;
}

SGMatrix<float64_t> NystromD::compute_G_mm()
{
	auto system_size = get_system_size();
	auto D = get_num_dimensions();

	SGMatrix<float64_t> G_mm(system_size, system_size);

#pragma omp parallel for
	for (auto idx_l=0; idx_l<m_basis_inds.vlen; idx_l++)
	{
		auto ai = idx_to_ai(m_basis_inds[idx_l], D);
		auto a = ai.first;
		auto i = ai.second;

		for (auto idx_k=0; idx_k<m_basis_inds.vlen; idx_k++)
		{
			auto bj = idx_to_ai(m_basis_inds[idx_k], D);
			auto b = bj.first;
			auto j = bj.second;

			G_mm(idx_k, idx_l) = m_kernel->dx_dy_component(b, a, j, i);
		}
	}

	return G_mm;
}

bool NystromD::basis_is_subsampled_data() const
{
	return m_data == m_basis;
}

SGMatrix<float64_t> NystromD::subsample_G_mm_from_G_mn(const SGMatrix<float64_t>& G_mn) const
{
	auto system_size = get_system_size();

	SGMatrix<float64_t> G_mm(system_size, system_size);
	for (auto idx_l=0; idx_l<m_basis_inds.vlen; idx_l++)
	{
		for (auto idx_k=0; idx_k<system_size; idx_k++)
			G_mm(idx_k,idx_l) = G_mn(idx_k, m_basis_inds[idx_l]);
	}

	return G_mm;
}

std::pair<index_t, index_t> NystromD::idx_to_ai(index_t idx, index_t D)
{
	return std::pair<index_t, index_t>(idx / D, idx%D);
}

float64_t NystromD::log_pdf(index_t idx_test) const
{
	auto D = get_num_dimensions();
	auto system_size = get_system_size();

	float64_t beta_sum = 0;

	for (auto idx_l=0; idx_l<system_size; idx_l++)
	{
		auto ai = idx_to_ai(m_basis_inds[idx_l], D);
		auto a = ai.first;
		auto i = ai.second;

		auto grad_x_xa = m_kernel->dx_component(a, idx_test, i);
		beta_sum += m_beta[idx_l]*grad_x_xa;
	}

	auto result = beta_sum;
	return result;
}

SGVector<float64_t> NystromD::grad(index_t idx_test) const
{
	auto D = get_num_dimensions();
	auto system_size = get_system_size();

	SGVector<float64_t> beta_grad_sum(D);
	beta_grad_sum.zero();

	for (auto idx_l=0; idx_l<system_size; idx_l++)
	{
		auto ai = idx_to_ai(m_basis_inds[idx_l], D);
		auto a = ai.first;
		auto i = ai.second;

		auto left_arg_hessian = m_kernel->dx_i_dx_j_component(a, idx_test, i);

		// mimic dot product with beta segment, but only relevant components
		for (auto idx_k=0; idx_k<system_size; idx_k++)
		{
			auto bj = idx_to_ai(m_basis_inds[idx_k], D);
			auto b = bj.first;
			if (a!=b)
				continue;

			auto j = bj.second;
			beta_grad_sum[i] -= left_arg_hessian[j]*m_beta[idx_k];
		}
	}

	auto result = beta_grad_sum;
	return beta_grad_sum;
}

SGVector<float64_t> NystromD::get_beta_for_basis_point(index_t a) const
{
	auto D = get_num_dimensions();
	auto system_size = get_system_size();

	SGVector<float64_t> beta_a(D);
	beta_a.zero();
	for (auto idx_k=0; idx_k<system_size; idx_k++)
	{
		auto bj = idx_to_ai(m_basis_inds[idx_k], D);
		auto b = bj.first;
		auto j = bj.second;

		if (a!=b)
			continue;
		beta_a[j] = m_beta[idx_k];
	}

	return beta_a;
}

SGMatrix<float64_t> NystromD::hessian(index_t idx_test) const
{
	auto D = get_num_dimensions();
	auto system_size = get_system_size();

	SGMatrix<float64_t> beta_sum_hessian(D, D);
	beta_sum_hessian.zero();

	for (auto idx_l=0; idx_l<system_size; idx_l++)
	{
		auto ai = idx_to_ai(m_basis_inds[idx_l], D);
		auto a = ai.first;
		auto i = ai.second;

		// mimic beta vector for index a (all non-used components zero)
		auto beta_a = get_beta_for_basis_point(a);

		// mimic dot product with beta segment, but only relevant components
		for (auto idx_k=0; idx_k<system_size; idx_k++)
		{
			auto bj = idx_to_ai(m_basis_inds[idx_k], D);
			auto b = bj.first;
			if (a!=b)
				continue;

			auto j = bj.second;

			// call contains dot product of beta_a (with zero components)
			// with difference vector (all components)
			auto beta_hess_sum = m_kernel->dx_i_dx_j_dx_k_dot_vec_component(a, idx_test, beta_a, i, j);
			beta_sum_hessian(i,j) += beta_hess_sum;
		}
	}

	auto result = beta_sum_hessian;
	return result;
}

SGVector<float64_t> NystromD::hessian_diag(index_t idx_test) const
{
	auto D = get_num_dimensions();
	auto system_size = get_system_size();

	SGVector<float64_t> beta_sum_hessian_diag(D);
	beta_sum_hessian_diag.zero();

	for (auto idx_l=0; idx_l<system_size; idx_l++)
	{
		auto ai = idx_to_ai(m_basis_inds[idx_l], D);
		auto a = ai.first;
		auto i = ai.second;

		// mimic beta vector for index a (all non-used components zero)
		auto beta_a = get_beta_for_basis_point(a);

		auto beta_hess_sum = m_kernel->dx_i_dx_j_dx_k_dot_vec_component(a, idx_test, beta_a, i, i);
		beta_sum_hessian_diag[i] += beta_hess_sum;
	}

	auto result = beta_sum_hessian_diag;
	return result;
}