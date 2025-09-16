// Main Application Controller
class ScadaWebApp {
    constructor() {
        this.currentTab = 'dashboard';
        this.managers = {
            dashboard: window.dashboard,
            tags: window.tagsManager,
            opcua: window.opcuaManager,
            admin: window.adminManager
        };
        this.connectionCheckInterval = null;
    }

    init() {
        console.log('🚀 Initializing SCADA Web App...');
        this.setupTabNavigation();
        this.initializeManagers();
        this.startConnectionMonitoring();
        this.setupGlobalEventListeners();
        
        // Show initial tab
        this.showTab('dashboard');
        console.log('✅ SCADA Web App initialization complete');
    }

    setupTabNavigation() {
        const navLinks = document.querySelectorAll('.nav-link[data-tab]');
        navLinks.forEach(link => {
            link.addEventListener('click', (e) => {
                e.preventDefault();
                const tabName = link.getAttribute('data-tab');
                this.showTab(tabName);
            });
        });
    }

    showTab(tabName) {
        // Stop current tab manager
        if (this.managers[this.currentTab] && this.managers[this.currentTab].stop) {
            this.managers[this.currentTab].stop();
        }

        // Hide all tab contents
        const allTabs = document.querySelectorAll('.tab-content');
        allTabs.forEach(tab => {
            tab.style.display = 'none';
        });

        // Show selected tab
        const targetTab = document.getElementById(`tab-${tabName}`);
        if (targetTab) {
            targetTab.style.display = 'block';
            targetTab.classList.add('fade-in');
        }

        // Update navigation
        const navLinks = document.querySelectorAll('.nav-link[data-tab]');
        navLinks.forEach(link => {
            link.classList.remove('active');
            if (link.getAttribute('data-tab') === tabName) {
                link.classList.add('active');
            }
        });

        // Start new tab manager
        this.currentTab = tabName;
        if (this.managers[tabName] && this.managers[tabName].start) {
            this.managers[tabName].start();
        }

        // Load data for specific tabs
        this.loadTabData(tabName);
    }

    async loadTabData(tabName) {
        console.log(`🔄 Loading data for tab: ${tabName}`);
        switch (tabName) {
            case 'dashboard':
                // Dashboard auto-refreshes, no manual load needed
                break;
            case 'tags':
                console.log('📋 Loading tags data...');
                if (this.managers.tags && this.managers.tags.loadTags) {
                    await this.managers.tags.loadTags();
                } else {
                    console.error('❌ Tags manager not available!');
                }
                break;
            case 'opcua':
                // OPC UA manager auto-loads when started
                break;
            case 'admin':
                // Admin panel loads on demand
                break;
        }
    }

    initializeManagers() {
        Object.values(this.managers).forEach(manager => {
            if (manager && manager.init) {
                manager.init();
            }
        });

        // Setup modal event handlers
        this.setupModalHandlers();
    }

    setupModalHandlers() {
        // Clear tag modal when hidden
        const tagModal = document.getElementById('tagModal');
        if (tagModal) {
            tagModal.addEventListener('hidden.bs.modal', () => {
                this.managers.tags.clearModal();
            });
        }
    }

    startConnectionMonitoring() {
        // Check connection status every 30 seconds
        this.connectionCheckInterval = setInterval(() => {
            this.checkConnection();
        }, 30000);

        // Initial check
        this.checkConnection();
    }

    async checkConnection() {
        console.log('🔍 Checking API connection...');
        try {
            const result = await scadaAPI.getHealth();
            console.log('✅ API Health Check successful:', result);
            this.updateConnectionIndicators(true);
        } catch (error) {
            console.error('❌ API Health Check failed:', error);
            this.updateConnectionIndicators(false);
        }
    }

    updateConnectionIndicators(isConnected) {
        console.log(`🔄 Updating connection indicators: ${isConnected ? 'Connected' : 'Disconnected'}`);
        const statusBadge = document.getElementById('connection-status');
        if (statusBadge) {
            if (isConnected) {
                statusBadge.textContent = 'Conectado';
                statusBadge.className = 'badge bg-success';
                console.log('✅ Status badge updated to Connected');
            } else {
                statusBadge.textContent = 'Desconectado';
                statusBadge.className = 'badge bg-danger';
                console.log('❌ Status badge updated to Disconnected');
            }
        } else {
            console.warn('⚠️  Status badge element not found!');
        }

        // Update any other connection indicators
        const indicators = document.querySelectorAll('.connection-indicator');
        indicators.forEach(indicator => {
            if (isConnected) {
                indicator.classList.remove('offline');
                indicator.classList.add('online');
            } else {
                indicator.classList.remove('online');
                indicator.classList.add('offline');
            }
        });
    }

    setupGlobalEventListeners() {
        // Global keyboard shortcuts
        document.addEventListener('keydown', (e) => {
            // Ctrl/Cmd + R: Refresh current tab
            if ((e.ctrlKey || e.metaKey) && e.key === 'r') {
                e.preventDefault();
                this.refreshCurrentTab();
            }

            // Ctrl/Cmd + 1-4: Switch tabs
            if ((e.ctrlKey || e.metaKey) && e.key >= '1' && e.key <= '4') {
                e.preventDefault();
                const tabNames = ['dashboard', 'tags', 'opcua', 'admin'];
                const tabIndex = parseInt(e.key) - 1;
                if (tabNames[tabIndex]) {
                    this.showTab(tabNames[tabIndex]);
                }
            }
        });

        // Global error handler
        window.addEventListener('unhandledrejection', (event) => {
            console.error('Unhandled promise rejection:', event.reason);
            this.showGlobalError('Error inesperado: ' + event.reason.message);
        });

        // Window resize handler
        window.addEventListener('resize', () => {
            this.handleResize();
        });

        // Visibility change handler
        document.addEventListener('visibilitychange', () => {
            if (!document.hidden) {
                // Page became visible, refresh data
                setTimeout(() => this.refreshCurrentTab(), 1000);
            }
        });
    }

    refreshCurrentTab() {
        const manager = this.managers[this.currentTab];
        
        switch (this.currentTab) {
            case 'dashboard':
                if (manager && manager.refreshData) {
                    manager.refreshData();
                }
                break;
            case 'tags':
                if (manager && manager.loadTags) {
                    manager.loadTags();
                }
                break;
            case 'opcua':
                if (manager && manager.loadOpcuaTable) {
                    manager.loadOpcuaTable();
                }
                break;
            case 'admin':
                if (manager && manager.loadBackups) {
                    manager.loadBackups();
                }
                break;
        }
    }

    handleResize() {
        // Redraw charts if they exist
        if (this.managers.dashboard && this.managers.dashboard.charts) {
            Object.values(this.managers.dashboard.charts).forEach(chart => {
                if (chart && chart.resize) {
                    chart.resize();
                }
            });
        }
    }

    showGlobalError(message) {
        const toast = document.createElement('div');
        toast.className = 'alert alert-danger alert-dismissible position-fixed top-0 end-0 m-3';
        toast.style.zIndex = '10000';
        toast.innerHTML = `
            <i class="fas fa-exclamation-circle me-2"></i>
            <strong>Error del Sistema:</strong> ${message}
            <button type="button" class="btn-close" data-bs-dismiss="alert"></button>
        `;
        
        document.body.appendChild(toast);
        
        setTimeout(() => {
            if (toast.parentNode) {
                toast.remove();
            }
        }, 8000);
    }

    showGlobalSuccess(message) {
        const toast = document.createElement('div');
        toast.className = 'alert alert-success alert-dismissible position-fixed top-0 end-0 m-3';
        toast.style.zIndex = '10000';
        toast.innerHTML = `
            <i class="fas fa-check-circle me-2"></i>
            ${message}
            <button type="button" class="btn-close" data-bs-dismiss="alert"></button>
        `;
        
        document.body.appendChild(toast);
        
        setTimeout(() => {
            if (toast.parentNode) {
                toast.remove();
            }
        }, 5000);
    }

    // Public API methods
    getCurrentTab() {
        return this.currentTab;
    }

    getConnectionStatus() {
        return scadaAPI.getConnectionStatus();
    }

    async performSystemCheck() {
        try {
            const health = await scadaAPI.getHealth();
            const status = await scadaAPI.getStatus();
            
            return {
                healthy: health.status === 'healthy',
                version: status.version,
                uptime: status.uptime,
                memory_usage: status.memory_usage,
                cpu_usage: status.cpu_usage
            };
        } catch (error) {
            return {
                healthy: false,
                error: error.message
            };
        }
    }

    destroy() {
        // Cleanup method for proper disposal
        if (this.connectionCheckInterval) {
            clearInterval(this.connectionCheckInterval);
        }

        Object.values(this.managers).forEach(manager => {
            if (manager && manager.stop) {
                manager.stop();
            }
        });
    }
}

// Initialize application when DOM is loaded
document.addEventListener('DOMContentLoaded', () => {
    // Create global app instance
    window.scadaApp = new ScadaWebApp();
    
    // Initialize the application
    window.scadaApp.init();
    
    // Add some global utility functions
    window.showToast = (message, type = 'info') => {
        const alertClass = type === 'success' ? 'alert-success' : 
                          type === 'danger' ? 'alert-danger' : 
                          type === 'warning' ? 'alert-warning' : 'alert-info';
        
        const toast = document.createElement('div');
        toast.className = `alert ${alertClass} alert-dismissible position-fixed top-0 end-0 m-3`;
        toast.style.zIndex = '9999';
        toast.innerHTML = `
            ${message}
            <button type="button" class="btn-close" data-bs-dismiss="alert"></button>
        `;
        
        document.body.appendChild(toast);
        
        setTimeout(() => {
            if (toast.parentNode) {
                toast.remove();
            }
        }, 5000);
    };

    // Log successful initialization
    console.log('🚀 SCADA Web Interface initialized successfully');
});