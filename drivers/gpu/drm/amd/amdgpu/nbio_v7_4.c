/*
 * Copyright 2018 Advanced Micro Devices, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 */
#include "amdgpu.h"
#include "amdgpu_atombios.h"
#include "nbio_v7_4.h"

#include "nbio/nbio_7_4_offset.h"
#include "nbio/nbio_7_4_sh_mask.h"

#define smnNBIF_MGCG_CTRL_LCLK	0x1013a21c

#define smnCPM_CONTROL                                                                                  0x11180460
#define smnPCIE_CNTL2                                                                                   0x11180070

static u32 nbio_v7_4_get_rev_id(struct amdgpu_device *adev)
{
    u32 tmp = RREG32_SOC15(NBIO, 0, mmRCC_DEV0_EPF0_STRAP0);

	tmp &= RCC_DEV0_EPF0_STRAP0__STRAP_ATI_REV_ID_DEV0_F0_MASK;
	tmp >>= RCC_DEV0_EPF0_STRAP0__STRAP_ATI_REV_ID_DEV0_F0__SHIFT;

	return tmp;
}

static void nbio_v7_4_mc_access_enable(struct amdgpu_device *adev, bool enable)
{
	if (enable)
		WREG32_SOC15(NBIO, 0, mmBIF_FB_EN,
			BIF_FB_EN__FB_READ_EN_MASK | BIF_FB_EN__FB_WRITE_EN_MASK);
	else
		WREG32_SOC15(NBIO, 0, mmBIF_FB_EN, 0);
}

static void nbio_v7_4_hdp_flush(struct amdgpu_device *adev,
				struct amdgpu_ring *ring)
{
	if (!ring || !ring->funcs->emit_wreg)
		WREG32_SOC15_NO_KIQ(NBIO, 0, mmHDP_MEM_COHERENCY_FLUSH_CNTL, 0);
	else
		amdgpu_ring_emit_wreg(ring, SOC15_REG_OFFSET(
			NBIO, 0, mmHDP_MEM_COHERENCY_FLUSH_CNTL), 0);
}

static u32 nbio_v7_4_get_memsize(struct amdgpu_device *adev)
{
	return RREG32_SOC15(NBIO, 0, mmRCC_CONFIG_MEMSIZE);
}

static void nbio_v7_4_sdma_doorbell_range(struct amdgpu_device *adev, int instance,
					  bool use_doorbell, int doorbell_index)
{
	u32 reg = instance == 0 ? SOC15_REG_OFFSET(NBIO, 0, mmBIF_SDMA0_DOORBELL_RANGE) :
			SOC15_REG_OFFSET(NBIO, 0, mmBIF_SDMA1_DOORBELL_RANGE);

	u32 doorbell_range = RREG32(reg);

	if (use_doorbell) {
		doorbell_range = REG_SET_FIELD(doorbell_range, BIF_SDMA0_DOORBELL_RANGE, OFFSET, doorbell_index);
		doorbell_range = REG_SET_FIELD(doorbell_range, BIF_SDMA0_DOORBELL_RANGE, SIZE, 8);
	} else
		doorbell_range = REG_SET_FIELD(doorbell_range, BIF_SDMA0_DOORBELL_RANGE, SIZE, 0);

	WREG32(reg, doorbell_range);
}

static void nbio_v7_4_enable_doorbell_aperture(struct amdgpu_device *adev,
					       bool enable)
{
	WREG32_FIELD15(NBIO, 0, RCC_DOORBELL_APER_EN, BIF_DOORBELL_APER_EN, enable ? 1 : 0);
}

static void nbio_v7_4_enable_doorbell_selfring_aperture(struct amdgpu_device *adev,
							bool enable)
{

}

static void nbio_v7_4_ih_doorbell_range(struct amdgpu_device *adev,
					bool use_doorbell, int doorbell_index)
{
	u32 ih_doorbell_range = RREG32_SOC15(NBIO, 0 , mmBIF_IH_DOORBELL_RANGE);

	if (use_doorbell) {
		ih_doorbell_range = REG_SET_FIELD(ih_doorbell_range, BIF_IH_DOORBELL_RANGE, OFFSET, doorbell_index);
		ih_doorbell_range = REG_SET_FIELD(ih_doorbell_range, BIF_IH_DOORBELL_RANGE, SIZE, 2);
	} else
		ih_doorbell_range = REG_SET_FIELD(ih_doorbell_range, BIF_IH_DOORBELL_RANGE, SIZE, 0);

	WREG32_SOC15(NBIO, 0, mmBIF_IH_DOORBELL_RANGE, ih_doorbell_range);
}


static void nbio_v7_4_update_medium_grain_clock_gating(struct amdgpu_device *adev,
						       bool enable)
{
	//TODO: Add support for v7.4
}

static void nbio_v7_4_update_medium_grain_light_sleep(struct amdgpu_device *adev,
						      bool enable)
{
	uint32_t def, data;

	def = data = RREG32_PCIE(smnPCIE_CNTL2);
	if (enable && (adev->cg_flags & AMD_CG_SUPPORT_BIF_LS)) {
		data |= (PCIE_CNTL2__SLV_MEM_LS_EN_MASK |
			 PCIE_CNTL2__MST_MEM_LS_EN_MASK |
			 PCIE_CNTL2__REPLAY_MEM_LS_EN_MASK);
	} else {
		data &= ~(PCIE_CNTL2__SLV_MEM_LS_EN_MASK |
			  PCIE_CNTL2__MST_MEM_LS_EN_MASK |
			  PCIE_CNTL2__REPLAY_MEM_LS_EN_MASK);
	}

	if (def != data)
		WREG32_PCIE(smnPCIE_CNTL2, data);
}

static void nbio_v7_4_get_clockgating_state(struct amdgpu_device *adev,
					    u32 *flags)
{
	int data;

	/* AMD_CG_SUPPORT_BIF_MGCG */
	data = RREG32_PCIE(smnCPM_CONTROL);
	if (data & CPM_CONTROL__LCLK_DYN_GATE_ENABLE_MASK)
		*flags |= AMD_CG_SUPPORT_BIF_MGCG;

	/* AMD_CG_SUPPORT_BIF_LS */
	data = RREG32_PCIE(smnPCIE_CNTL2);
	if (data & PCIE_CNTL2__SLV_MEM_LS_EN_MASK)
		*flags |= AMD_CG_SUPPORT_BIF_LS;
}

static void nbio_v7_4_ih_control(struct amdgpu_device *adev)
{
	u32 interrupt_cntl;

	/* setup interrupt control */
	WREG32_SOC15(NBIO, 0, mmINTERRUPT_CNTL2, adev->dummy_page_addr >> 8);
	interrupt_cntl = RREG32_SOC15(NBIO, 0, mmINTERRUPT_CNTL);
	/* INTERRUPT_CNTL__IH_DUMMY_RD_OVERRIDE_MASK=0 - dummy read disabled with msi, enabled without msi
	 * INTERRUPT_CNTL__IH_DUMMY_RD_OVERRIDE_MASK=1 - dummy read controlled by IH_DUMMY_RD_EN
	 */
	interrupt_cntl = REG_SET_FIELD(interrupt_cntl, INTERRUPT_CNTL, IH_DUMMY_RD_OVERRIDE, 0);
	/* INTERRUPT_CNTL__IH_REQ_NONSNOOP_EN_MASK=1 if ring is in non-cacheable memory, e.g., vram */
	interrupt_cntl = REG_SET_FIELD(interrupt_cntl, INTERRUPT_CNTL, IH_REQ_NONSNOOP_EN, 0);
	WREG32_SOC15(NBIO, 0, mmINTERRUPT_CNTL, interrupt_cntl);
}

static u32 nbio_v7_4_get_hdp_flush_req_offset(struct amdgpu_device *adev)
{
	return SOC15_REG_OFFSET(NBIO, 0, mmGPU_HDP_FLUSH_REQ);
}

static u32 nbio_v7_4_get_hdp_flush_done_offset(struct amdgpu_device *adev)
{
	return SOC15_REG_OFFSET(NBIO, 0, mmGPU_HDP_FLUSH_DONE);
}

static u32 nbio_v7_4_get_pcie_index_offset(struct amdgpu_device *adev)
{
	return SOC15_REG_OFFSET(NBIO, 0, mmPCIE_INDEX2);
}

static u32 nbio_v7_4_get_pcie_data_offset(struct amdgpu_device *adev)
{
	return SOC15_REG_OFFSET(NBIO, 0, mmPCIE_DATA2);
}

static const struct nbio_hdp_flush_reg nbio_v7_4_hdp_flush_reg = {
	.ref_and_mask_cp0 = GPU_HDP_FLUSH_DONE__CP0_MASK,
	.ref_and_mask_cp1 = GPU_HDP_FLUSH_DONE__CP1_MASK,
	.ref_and_mask_cp2 = GPU_HDP_FLUSH_DONE__CP2_MASK,
	.ref_and_mask_cp3 = GPU_HDP_FLUSH_DONE__CP3_MASK,
	.ref_and_mask_cp4 = GPU_HDP_FLUSH_DONE__CP4_MASK,
	.ref_and_mask_cp5 = GPU_HDP_FLUSH_DONE__CP5_MASK,
	.ref_and_mask_cp6 = GPU_HDP_FLUSH_DONE__CP6_MASK,
	.ref_and_mask_cp7 = GPU_HDP_FLUSH_DONE__CP7_MASK,
	.ref_and_mask_cp8 = GPU_HDP_FLUSH_DONE__CP8_MASK,
	.ref_and_mask_cp9 = GPU_HDP_FLUSH_DONE__CP9_MASK,
	.ref_and_mask_sdma0 = GPU_HDP_FLUSH_DONE__SDMA0_MASK,
	.ref_and_mask_sdma1 = GPU_HDP_FLUSH_DONE__SDMA1_MASK,
};

static void nbio_v7_4_detect_hw_virt(struct amdgpu_device *adev)
{
	if (is_virtual_machine())	/* passthrough mode exclus sriov mod */
		adev->virt.caps |= AMDGPU_PASSTHROUGH_MODE;
}

static void nbio_v7_4_init_registers(struct amdgpu_device *adev)
{

}

const struct amdgpu_nbio_funcs nbio_v7_4_funcs = {
	.hdp_flush_reg = &nbio_v7_4_hdp_flush_reg,
	.get_hdp_flush_req_offset = nbio_v7_4_get_hdp_flush_req_offset,
	.get_hdp_flush_done_offset = nbio_v7_4_get_hdp_flush_done_offset,
	.get_pcie_index_offset = nbio_v7_4_get_pcie_index_offset,
	.get_pcie_data_offset = nbio_v7_4_get_pcie_data_offset,
	.get_rev_id = nbio_v7_4_get_rev_id,
	.mc_access_enable = nbio_v7_4_mc_access_enable,
	.hdp_flush = nbio_v7_4_hdp_flush,
	.get_memsize = nbio_v7_4_get_memsize,
	.sdma_doorbell_range = nbio_v7_4_sdma_doorbell_range,
	.enable_doorbell_aperture = nbio_v7_4_enable_doorbell_aperture,
	.enable_doorbell_selfring_aperture = nbio_v7_4_enable_doorbell_selfring_aperture,
	.ih_doorbell_range = nbio_v7_4_ih_doorbell_range,
	.update_medium_grain_clock_gating = nbio_v7_4_update_medium_grain_clock_gating,
	.update_medium_grain_light_sleep = nbio_v7_4_update_medium_grain_light_sleep,
	.get_clockgating_state = nbio_v7_4_get_clockgating_state,
	.ih_control = nbio_v7_4_ih_control,
	.init_registers = nbio_v7_4_init_registers,
	.detect_hw_virt = nbio_v7_4_detect_hw_virt,
};
