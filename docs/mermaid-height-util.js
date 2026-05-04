/**
 * MermaidHeightAdjuster - Utility class for managing Mermaid diagram scaling and sizing
 */
class MermaidHeightAdjuster {
    constructor() {
        this.scales = new Map();
        this.defaultScale = 100;
        this.minScale = 50;
        this.maxScale = 200;
    }

    /**
     * Setup tab switching handlers
     */
    setupTabSwitching() {
        document.addEventListener('click', (e) => {
            const tab = e.target.closest('.nav-tab');
            if (tab) {
                const tabId = tab.dataset.tab;
                this.onTabSwitch(tabId);
            }
        });
    }

    /**
     * Setup scale control button handlers
     */
    setupScaleControls() {
        document.addEventListener('click', (e) => {
            const scaleBtn = e.target.closest('[data-scale-delta]');
            const resetBtn = e.target.closest('[data-reset-scale]');

            if (scaleBtn) {
                const delta = parseInt(scaleBtn.dataset.scaleDelta, 10);
                const container = scaleBtn.closest('.diagram-container');
                this.changeScale(delta, container);
            }

            if (resetBtn) {
                const container = resetBtn.closest('.diagram-container');
                this.resetScale(container);
            }
        });
    }

    /**
     * Handle tab switch event
     */
    onTabSwitch(tabId) {
        const tabContent = document.getElementById(tabId);
        if (!tabContent) return;

        const containers = tabContent.querySelectorAll('.diagram-container');
        containers.forEach(container => {
            setTimeout(() => this.adjustDiagram(container), 150);
        });
    }

    /**
     * Adjust diagram container size based on SVG bounding box
     */
    adjustDiagram(container) {
        if (!container) return;

        const svg = container.querySelector('svg');
        if (!svg) return;

        try {
            const bbox = svg.getBBox();
            const padding = 60;

            const minWidth = Math.max(bbox.width + padding, 300);
            const minHeight = Math.max(bbox.height + padding, 200);

            container.style.minWidth = `${minWidth}px`;
            container.style.minHeight = `${minHeight}px`;

            svg.style.width = '100%';
            svg.style.height = '100%';

            const currentScale = this.getScale(container);
            this.applyScale(container, currentScale);
        } catch (e) {
            console.warn('Failed to adjust diagram:', e);
        }
    }

    /**
     * Get current scale for a container
     */
    getScale(container) {
        const display = container.querySelector('.scale-display');
        if (!display) return this.defaultScale;

        const containerId = this.getContainerId(container);
        if (this.scales.has(containerId)) {
            return this.scales.get(containerId);
        }

        return this.defaultScale;
    }

    /**
     * Generate unique ID for container
     */
    getContainerId(container) {
        const diagram = container.querySelector('.mermaid');
        return diagram?.id || container.id || Math.random().toString(36);
    }

    /**
     * Change scale by delta
     */
    changeScale(delta, container) {
        if (!container) {
            container = document.querySelector('.tab-content.active .diagram-container');
        }
        if (!container) return;

        const containerId = this.getContainerId(container);
        let currentScale = this.scales.get(containerId) || this.defaultScale;
        let newScale = currentScale + delta;

        newScale = Math.max(this.minScale, Math.min(this.maxScale, newScale));

        this.scales.set(containerId, newScale);
        this.applyScale(container, newScale);
        this.updateScaleDisplay(container, newScale);
    }

    /**
     * Reset scale to default
     */
    resetScale(container) {
        if (!container) {
            container = document.querySelector('.tab-content.active .diagram-container');
        }
        if (!container) return;

        const containerId = this.getContainerId(container);
        this.scales.set(containerId, this.defaultScale);
        this.applyScale(container, this.defaultScale);
        this.updateScaleDisplay(container, this.defaultScale);
    }

    /**
     * Apply scale to diagram
     */
    applyScale(container, scale) {
        const mermaid = container.querySelector('.mermaid');
        if (!mermaid) return;

        const scaleDecimal = scale / 100;
        mermaid.style.transform = `scale(${scaleDecimal})`;
        mermaid.style.transformOrigin = 'top left';

        const svg = container.querySelector('svg');
        if (svg && svg.style.display !== 'none') {
            try {
                const bbox = svg.getBBox();
                const scaledWidth = bbox.width * scaleDecimal;
                const scaledHeight = bbox.height * scaleDecimal;

                container.style.minWidth = `${Math.max(scaledWidth + 60, 300)}px`;
                container.style.minHeight = `${Math.max(scaledHeight + 60, 200)}px`;
            } catch (e) {
                console.warn('Failed to calculate scaled size:', e);
            }
        }
    }

    /**
     * Update scale display text
     */
    updateScaleDisplay(container, scale) {
        const display = container?.querySelector('.scale-display');
        if (display) {
            display.textContent = `${scale}%`;
        }
    }
}

// Export for module systems or global use
if (typeof module !== 'undefined' && module.exports) {
    module.exports = { MermaidHeightAdjuster };
}
