import app.freerouting.Freerouting;
import app.freerouting.core.RoutingJob;
import app.freerouting.management.RoutingJobScheduler;
import java.time.Duration;
import java.time.Instant;

/** Bound Freerouting 2.1's effective per-job stop pass and request a graceful stop. */
public class BoundedFreerouting {
    public static void main(String[] args) {
        Thread watchdog = new Thread(() -> {
            while (true) {
                try {
                    for (RoutingJob job : RoutingJobScheduler.getInstance().listJobs()) {
                        if (job.routerSettings != null) {
                            // maxPasses alone does not update the imported DSN's stop pass.
                            job.routerSettings.set_stop_pass_no(30);
                            job.routerSettings.setRunOptimizer(false);
                            job.routerSettings.optimizer.maxPasses = 1;
                        }
                        if (job.thread != null && job.startedAt != null
                                && Duration.between(job.startedAt, Instant.now()).getSeconds() >= 90) {
                            job.thread.request_stop_auto_router();
                        }
                    }
                    Thread.sleep(200);
                } catch (InterruptedException interrupted) {
                    return;
                } catch (RuntimeException updatingQueue) {
                    // The scheduler may change its queue during the snapshot.
                }
            }
        }, "strip-routing-limit");
        watchdog.setDaemon(true);
        watchdog.start();
        Freerouting.main(args);
    }
}
