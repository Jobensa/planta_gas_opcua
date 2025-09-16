// Dashboard Module
class Dashboard {
    constructor() {
        this.charts = {};
        this.refreshInterval = null;
        this.refreshRate = 5000; // 5 seconds
        this.isActive = false;
    }

    init() {
        this.setupEventListeners();
        this.initCharts();
    }

    setupEventListeners() {
        // Auto-refresh toggle (could be added to UI)
        document.addEventListener('visibilitychange', () => {
            if (document.hidden) {
                this.stop();
            } else if (this.isActive) {
                this.start();
            }
        });
    }

    start() {
        this.isActive = true;
        this.refreshData();
        this.refreshInterval = setInterval(() => {
            this.refreshData();
        }, this.refreshRate);
    }

    stop() {
        this.isActive = false;
        if (this.refreshInterval) {
            clearInterval(this.refreshInterval);
            this.refreshInterval = null;
        }
    }

    async refreshData() {
        try {
            // Update connection status first
            this.updateConnectionStatus();

            // Fetch all dashboard data concurrently
            const promises = [
                this.updateSystemStatus(),
                this.updateSystemInfo(),
                this.updateMetrics(),
                this.updateRecentActivity()
            ];

            await Promise.allSettled(promises);
        } catch (error) {
            console.error('Dashboard refresh error:', error);
            this.showError('Error actualizando dashboard: ' + scadaAPI.formatError(error));
        }
    }

    updateConnectionStatus() {
        const status = scadaAPI.getConnectionStatus();
        const statusBadge = document.getElementById('connection-status');
        
        if (statusBadge) {
            if (status.online) {
                statusBadge.textContent = 'Conectado';
                statusBadge.className = 'badge bg-success';
            } else {
                statusBadge.textContent = 'Desconectado';
                statusBadge.className = 'badge bg-danger';
            }
        }
    }

    async updateSystemStatus() {
        try {
            const statistics = await scadaAPI.getStatistics();
            
            // Update status cards
            this.updateElement('active-tags-count', statistics.total_tags || 0);
            this.updateElement('opcua-connections', statistics.opcua_connections || 0);
            this.updateElement('monitored-vars', statistics.monitored_variables || 0);
            this.updateElement('active-alarms', statistics.active_alarms || 0);
            
        } catch (error) {
            console.warn('Could not update system status:', error);
            // Set default values when API is not available
            this.updateElement('active-tags-count', '-');
            this.updateElement('opcua-connections', '-');
            this.updateElement('monitored-vars', '-');
            this.updateElement('active-alarms', '-');
        }
    }

    async updateSystemInfo() {
        try {
            const status = await scadaAPI.getStatus();
            
            this.updateElement('version', status.version || '1.0.0');
            this.updateElement('uptime', this.formatUptime(status.uptime || 0));
            this.updateElement('memory-usage', this.formatMemory(status.memory_usage || 0));
            this.updateElement('cpu-usage', this.formatPercentage(status.cpu_usage || 0));
            
        } catch (error) {
            console.warn('Could not update system info:', error);
            this.updateElement('version', '1.0.0');
            this.updateElement('uptime', '-');
            this.updateElement('memory-usage', '-');
            this.updateElement('cpu-usage', '-');
        }
    }

    async updateMetrics() {
        try {
            const statistics = await scadaAPI.getStatistics();
            this.updateSystemMetricsChart(statistics);
        } catch (error) {
            console.warn('Could not update metrics chart:', error);
        }
    }

    async updateRecentActivity() {
        const activityContainer = document.getElementById('recent-activity');
        if (!activityContainer) return;

        try {
            // In a real implementation, this would fetch from a logs endpoint
            // For now, we'll generate some sample activity based on system status
            const activities = await this.generateSampleActivity();
            
            activityContainer.innerHTML = '';
            activities.forEach(activity => {
                const item = this.createActivityItem(activity);
                activityContainer.appendChild(item);
            });
            
        } catch (error) {
            console.warn('Could not update activity:', error);
            activityContainer.innerHTML = '<div class="text-muted">No hay actividad reciente disponible</div>';
        }
    }

    async generateSampleActivity() {
        const activities = [];
        const now = new Date();
        
        try {
            const health = await scadaAPI.getHealth();
            if (health.status === 'healthy') {
                activities.push({
                    type: 'success',
                    message: 'Sistema funcionando correctamente',
                    time: now
                });
            }
        } catch (error) {
            activities.push({
                type: 'danger',
                message: 'Error de conexión con el servidor',
                time: now
            });
        }

        // Add some more sample activities
        activities.push(
            {
                type: 'info',
                message: 'Actualización de métricas completada',
                time: new Date(now.getTime() - 2 * 60000)
            },
            {
                type: 'warning',
                message: 'Validación de configuración pendiente',
                time: new Date(now.getTime() - 5 * 60000)
            }
        );

        return activities;
    }

    createActivityItem(activity) {
        const item = document.createElement('div');
        item.className = `activity-item ${activity.type}`;
        
        const icon = this.getActivityIcon(activity.type);
        const timeStr = this.formatTime(activity.time);
        
        item.innerHTML = `
            <div class="d-flex justify-content-between align-items-start">
                <div>
                    <i class="${icon} me-2"></i>
                    ${activity.message}
                </div>
                <small class="activity-time">${timeStr}</small>
            </div>
        `;
        
        return item;
    }

    getActivityIcon(type) {
        const icons = {
            success: 'fas fa-check-circle text-success',
            warning: 'fas fa-exclamation-triangle text-warning',
            danger: 'fas fa-times-circle text-danger',
            info: 'fas fa-info-circle text-info'
        };
        return icons[type] || 'fas fa-circle';
    }

    initCharts() {
        this.initSystemMetricsChart();
    }

    initSystemMetricsChart() {
        const ctx = document.getElementById('systemMetricsChart');
        if (!ctx) return;

        this.charts.systemMetrics = new Chart(ctx, {
            type: 'line',
            data: {
                labels: [],
                datasets: [{
                    label: 'CPU (%)',
                    data: [],
                    borderColor: '#007bff',
                    backgroundColor: 'rgba(0, 123, 255, 0.1)',
                    tension: 0.4
                }, {
                    label: 'Memoria (%)',
                    data: [],
                    borderColor: '#28a745',
                    backgroundColor: 'rgba(40, 167, 69, 0.1)',
                    tension: 0.4
                }, {
                    label: 'Tags Activos',
                    data: [],
                    borderColor: '#ffc107',
                    backgroundColor: 'rgba(255, 193, 7, 0.1)',
                    tension: 0.4,
                    yAxisID: 'y1'
                }]
            },
            options: {
                responsive: true,
                maintainAspectRatio: false,
                plugins: {
                    legend: {
                        position: 'top',
                    },
                    title: {
                        display: false
                    }
                },
                scales: {
                    x: {
                        type: 'time',
                        time: {
                            unit: 'minute',
                            displayFormats: {
                                minute: 'HH:mm'
                            }
                        }
                    },
                    y: {
                        beginAtZero: true,
                        max: 100,
                        title: {
                            display: true,
                            text: 'Porcentaje'
                        }
                    },
                    y1: {
                        type: 'linear',
                        display: true,
                        position: 'right',
                        title: {
                            display: true,
                            text: 'Cantidad'
                        },
                        grid: {
                            drawOnChartArea: false,
                        },
                    }
                }
            }
        });
    }

    updateSystemMetricsChart(statistics) {
        const chart = this.charts.systemMetrics;
        if (!chart) return;

        const now = new Date();
        const maxDataPoints = 20;

        // Add new data point
        chart.data.labels.push(now);
        chart.data.datasets[0].data.push(Math.random() * 80 + 10); // CPU simulation
        chart.data.datasets[1].data.push(Math.random() * 60 + 20); // Memory simulation
        chart.data.datasets[2].data.push(statistics.total_tags || 0); // Actual tags count

        // Remove old data points
        if (chart.data.labels.length > maxDataPoints) {
            chart.data.labels.shift();
            chart.data.datasets.forEach(dataset => {
                dataset.data.shift();
            });
        }

        chart.update('none');
    }

    // Utility methods
    updateElement(id, value) {
        const element = document.getElementById(id);
        if (element) {
            element.textContent = value;
        }
    }

    formatUptime(seconds) {
        const hours = Math.floor(seconds / 3600);
        const minutes = Math.floor((seconds % 3600) / 60);
        return `${hours}h ${minutes}m`;
    }

    formatMemory(bytes) {
        const mb = bytes / (1024 * 1024);
        return `${mb.toFixed(1)} MB`;
    }

    formatPercentage(value) {
        return `${value.toFixed(1)}%`;
    }

    formatTime(date) {
        return date.toLocaleTimeString('es-ES', {
            hour: '2-digit',
            minute: '2-digit'
        });
    }

    showError(message) {
        // Could implement a toast notification system here
        console.error(message);
    }
}

// Global dashboard instance
window.dashboard = new Dashboard();